#include <errno.h>
#include <assert.h>
#include <sys/time.h>
#include <stdio.h>

#include "net.h"
#include "thread.h"
#include "common.h"
#include "protocol.h"
#include "uin_index.h"
#include "min_heap.h"
#include "hash.h"


class main_thread
{
public:
    static const int state_can_read = 1;
    static const int state_can_write = 2;
    
    void init();
    void run();
    
    block* get_available_block(unsigned int size)
    {
        if (size > BLOCK_SIZE) return NULL;
        for (int i = bl.size(); i >= 0; i --)
            if (bl[i].usable_room() >= size)
                return &bl[i];
        
        block b;
        b.create(BLOCK_SIZE);
        bl.push_back(b);
        return &bl[bl.size()-1];
    }
    
    void insert(unsigned int key, unsigned int bytes, char *data)
    {
        bool exist = false;
        hash::knot *k = ht.insert(key, 0, exist);
        if (exist) return;
        
        block *b = get_available_block(bytes);
        void *dest = b->apply();
        memcpy(dest, data, bytes);
        b->eat(bytes);
        
        k->arg = dest;
    }
    
    unsigned int key_count()
    {
        return ht.key_count();
    }
    
    void start_all_data_threads()
    {
        assure_start_worker_thread(wtl);
    }
    void stop_all_data_threads()
    {
        assure_stop_worker_thread(wtl);
    }
    
    //更新状态为可读
    void update_to_can_read()
    {
        mt_lock.lock();
        if (state == state_can_write)
        {
            stop_all_data_threads();
            while (wating_bl.size() > 0)
                read_block();
            state = state_can_read;
            start_all_data_threads();
        }
        mt_lock.unlock();
    }
    void update_to_can_write(bool clear)
    {
        mt_lock.lock();
        if (state == state_can_read)
        {
            stop_all_data_threads();
            if (clear)
                clear_the_whole_blocks();
            state = state_can_write;
            start_all_data_threads();
        }
        mt_lock.unlock();
    }
    
    bool is_state_can_read()
    {
        return state == state_can_read;
    }
    
    
    main_thread()
    {
        state = state_can_read;
        thread_count = 10;
        working_cnt = 0;
        max_simultaneous_read = 0;
        max_simultaneous_write = 0;
    }
    
    void get_pending_blocks();
    void process_one_block(block& b);
    void read_block();
    void clear_the_whole_blocks()
    {
        ht.clear();

        unsigned clear_bytes = 0;
        unsigned cap_Mbytes = 0;

        unsigned block_count = bl.size();

        for (unsigned i = 0; i < bl.size(); ++i)
        {
            clear_bytes += bl[i].offset;
            cap_Mbytes += (bl[i].cap/(1024*1024));
            bl[i].clear();
        }

        printf("clear_the_whole_blocks block_cnt:%u clear_bytes:%u cap_bytes:%uMB\n", block_count, clear_bytes, cap_Mbytes);
    }
    
    
    int state;
    
    worker_thread *ct;
    vector<worker_thread*> wtl;
    
    mutex ltn_sock_lock;
    int data_ltn_sock;
    
    hash_table ht;
    vector<block> bl;
    vector<block> wating_bl;
    
    int thread_count;
    mutex mt_lock;
    
    fast_lock running_cnt_lock;
    volatile int working_cnt;
    volatile int max_simultaneous_read;
    volatile int max_simultaneous_write;
    
    void worker_thread_start_work(int type = 0)
    {
        running_cnt_lock.lock();
        working_cnt += 1;
        if (type == 1)
        {
            if (working_cnt > max_simultaneous_read)
                max_simultaneous_read = working_cnt;
        }
        else if (type == 2)
        {
            if (working_cnt > max_simultaneous_write)
                max_simultaneous_write = working_cnt;
        }
        running_cnt_lock.unlock();
    }

    void worker_thread_stop_work()
    {
        running_cnt_lock.lock();
        working_cnt -= 1;
        running_cnt_lock.unlock();
    }
};


#define barrier() __asm__ __volatile_("": : :"memory")
void response(int data_sock, int cmd, int err)
{
    char buf[12];
    response_header *rh = (response_header*)buf;
    rh->cmd = cmd;
    rh->err = err;
    rh->length = 12;
    
    assure_write(sock, buf, 12, 100);
    close(data_sock);
}
void response(int data_sock, int cmd, int err, void *data, unsigned int size)
{
    char buf[12];
    response_header *rh = (response_header*)buf;
    rh->cmd = cmd;
    rh->err = err;
    rh->length = 12+size;
    
    assure_write(data_sock, buf, 12, 100);
    if (size > 0)
        assure_write(data_sock, (char*)data, size, 100);
    close(data_sock);
}

void response(int data_sock, int cmd, int err, vector<iovec>& iov, vector<unsigned>& cum)
{
    char buf[12];
    response_header* rh = (response_header*)buf;
    rh->cmd = cmd;
    rh->err = err;

    unsigned body_bytes = 0;
    if (cum.size()>0)
        body_bytes = cum[cum.size()-1];
    rh->length = 12+body_bytes;

    assure_write(data_sock, buf, 12, 100);

    if (body_bytes > 0)
        assure_write(data_sock, iov, cum, 2000);
    
    close(data_sock);
}


class data_thread : public worker_thread
{
public:
    main_thread *mt;
    vector<block> bl;
    
    fast_lock write_lock;
    vector<block> to_main_block;
    
    char reqbuf[1024*1024];
    
    data_thread()
    {
        mt = 0;
    }
    
    void data_put_main(block *b)
    {
        write_lock.lock();
        to_main_block.push_back(b);
        write_lock.unlock();
    }
    
    void main_get_data(vector<block> &bl)
    {
        write_lock.lock();
        for (int i = 0; i < to_main_block.size(); i ++)
            bl.push_back(to_main_block[i]);
        to_main_block.clear();
        write_lock.unlock();
    }
    
    void try_to_accept_a_client(int& data_sock, bool& error_happened)
    {
        data_sock = -1;
        error_happened = false;

        struct sockaddr_in client_addr;
        socklen_t al = sizeof(client_addr);

        mt->ltn_sock_lock.lock();
        while (1)
        {
            data_sock = accept(mt->data_ltn_sock, (struct sockaddr*)&client_addr, &al);
            if (data_sock > 0) break;
            
            if (errno == EINTR) continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            
            error_happened = true;
            printf("[ERROR] accept sock:%d errno:%d emsg:%s\n", data_sock, errno, strerror(errno));
            break;
        }
        mt->ltn_sock_lock.unlock();
    }
    
    void process_read(int data_sock)
    {
        set_nonblock(data_sock);
        set_sndbuf_size(data_sock, 256*1024);
        
        unsigned bytes = 0;
        int ret = 0;

        char head_buf[8] = {0};
        ret = assure_read_package(data_sock, 8, head_buf,  100);
        if (ret < 0)
        {
            close(data_sock);
            return;
        }
	
        request_header* rh = (request_header*)head_buf;    
        if (rh->cmd != CMD_CACHE_GET_BATCH)
        {
            response(data_sock, rh->cmd, ERR_FORBID_BY_CUR_STATE);
            return;
        }

        if (rh->length < 8)
        {
            response(data_sock, rh->cmd, ERR_INVALID_REQUEST_SIZE);
            return;
        }

        if (rh->length-8 > sizeof(reqbuf))
        {
            response(data_sock, rh->cmd, ERR_TOOLARGE_REQUEST);
            return;
        }
	    
        ret = assure_read_package(data_sock, rh->length-8, reqbuf, 10000);
        if (ret < 0)
        {
            if (ret  == -3)
                response(data_sock, rh->cmd, ERR_TIMEOUT);
            else
                close(data_sock);
            return;
        }

        unsigned uinlen = (rh->length-8-sizeof(unsigned))/sizeof(unsigned);

        unsigned* keys = (unsigned*)(reqbuf+sizeof(unsigned));
        vector<iovec> tosend;
        vector<unsigned> tosend_cumulate;
        unsigned total_bytes = 0;
        for (unsigned i = 0; i < uinlen; ++i)
        {
            iovec iv;
            unsigned key = keys[i];
            
            hash::knot* k = mt->ht.find(key);

            if (k == NULL || k->arg == NULL) continue;
            sns_header* sh = (sns_header*)k->arg;

            unsigned bytes = sh->len+2*sizeof(unsigned);
            total_bytes += bytes;

            iv.iov_base = sh;
            iv.iov_len = bytes;
            tosend.push_back(iv);
            unsigned cum = bytes;
            if (tosend_cumulate.size()>0)
                cum += tosend_cumulate[tosend_cumulate.size()-1];
            tosend_cumulate.push_back(cum);
        }

        unsigned cum_bytes = 0;
        if (tosend_cumulate.size()>0)
            cum_bytes = tosend_cumulate[tosend_cumulate.size()-1];

        //printf("[%05d] get sns batch uinlen:%u findlen:%u findbytes:%u cumbytes:%u\n", data_sock, uinlen, tosend.size(), total_bytes, cum_bytes);

        response(data_sock, rh->cmd, 0, tosend, tosend_cumulate);
    }
    
    virtual void run_read()
    {
        printf("data_thread::run_read\n");
        i_am_stoped = false; 
        while (!is_stop())
        {
            int data_sock = -1;
            bool error_happened = false;
            
            try_to_accept_a_client(data_sock, error_happened);
            
            if (error_happened || data_sock <= 0)
            {
                if (data_sock > 0) close(data_sock);
                else usleep(1000);
                continue;
            }
            
            mt->worker_thread_start_work(1);
            process_read(data_sock);
            mt->worker_thread_stop_work();
        }
        i_am_stoped = true;
    }
    
    void process_write(int data_sock)
    {
        //printf("[%05d] [INFO] new data sock\n", data_sock);
        set_nonblock(data_sock);
        set_sndbuf_size(data_sock, 256*1024);
        
        unsigned bytes = 0;
        int ret = 0;
        
        char head_buf[8] = {0};
        ret = assure_read_package(data_sock, 8, head_buf,  100);
        if (ret < 0)
        {
            close(data_sock);
            return;
        }
        
        request_header* rh = (request_header*)head_buf;
        if (rh->length < 8 || rh->length > LIMIT_REQUEST_SIZE)
        {
            response(data_sock, rh->cmd, ERR_INVALID_REQUEST_SIZE);
            return;
        }
        if (rh->cmd != CMD_CACHE_ADD_BATCH)
        {
            response(data_sock, rh->cmd, ERR_FORBID_BY_CUR_STATE);
            return;
        }
        
        //printf("[%05d] [INFO] request {cmd:%d,length:%d}\n", data_sock, rh->cmd, rh->length);
        block b;
        if (!b.create(rh->length-8))
        {
            response(data_sock, rh->cmd, ERR_NOMEM);
            return;
        }
        
        char* data = (char*)b.apply();
        ret = assure_read_package(data_sock, rh->length-8, data, 20000);
        if (ret < 0)
        {
            if (ret  == -3)
                response(data_sock, rh->cmd, ERR_TIMEOUT);
            else
                close(data_sock);
            b.free_memory();
            return;
        }
        
        b.eat(rh->length-8);
        //bl.push_back(b);
        data_put_main(b);
        response(data_sock, rh->cmd, 0);
        //printf("[%05d] [INFO] request done\n", data_sock);
    }
    
    
    virtual void run_write()
    {
        i_am_stoped = false; 
        printf("data_thread::run_write\n");
        while (!is_stop())
        {
            int data_sock = -1;
            bool error_happened = false;
            
            try_to_accept_a_client(data_sock, error_happened);
            if (error_happened || data_sock <= 0)
            {
                if (data_sock > 0) close(data_sock);
                else usleep(10);
                continue;
            }
            
            mt->worker_thread_start_work(2);
            process_write(data_sock);
            mt->worker_thread_stop_work();
        }
        i_am_stoped = true;
    }
    
    virtual void run()
    {
        if (mt->is_state_can_read()) run_read(); 
        else run_write();
    }
};

class cmd_thread : public worker_thread {
public:
    main_thread* mt;
    int cmd_ltn_sock;
    cmd_thread() {
        mt = 0;
    cmd_ltn_sock = -1;
    }

    virtual void run() {
        i_am_stoped = false; 
        while (!is_stop()) {            
            struct sockaddr_in client_addr;
            socklen_t al = sizeof(client_addr);
            int client_sock;

	    while (1) {
	        client_sock = accept(cmd_ltn_sock,  (struct sockaddr* )&client_addr, &al);
	        if (client_sock <= 0) {
		    if (errno == EINTR)
		        continue;
	            else if (errno == EAGAIN || errno == EWOULDBLOCK)
		        break;
		    else {
		        printf("[ERROR] cmd thread accept failed! errno:%d emsg:%s\n", errno, strerror(errno));
			_exit(1);
		    }
	        } else {
	            //process the
		    unsigned bytes = 0;
		    int ret = assure_read_package_length_msgpeek(client_sock, bytes, 100);
		    if (ret < 0) {
		        printf("[%05d] [ERROR] failed when get package length! ret:%d errno:%d\n", client_sock, ret, errno);
			break;
		    }

		    printf("[%05d] [INFO] package length:%u\n", client_sock, bytes);

		    char* buf = (char*)malloc(bytes);
		    ret = assure_read_package(client_sock, bytes, buf, 100);
		    if (ret < 0) {
		        free(buf);
		        printf("[%05d] [ERROR] failed when get package! ret:%d errno:%d\n", client_sock, ret, errno);
			break;
		    }


		    request_header* rh = (request_header*)buf;

		    printf("[%05d] [INFO] bytes:%u cmd:%d\n", client_sock, rh->length, rh->cmd);

		    switch(rh->cmd) {
		    case CMD_CACHE_UPDATE_STATE_UPDATING: {
		        mt->update_to_can_write(true);
			response(client_sock, rh->cmd, 0);
		    } break;
		    case CMD_CACHE_UPDATE_STATE_APPENDING: {
		        mt->update_to_can_write(false);
			response(client_sock, rh->cmd, 0);
		    } break;
		    case CMD_CACHE_UPDATE_STATE_ESTABLISHED: {
		        mt->update_to_can_read();
			response(client_sock, rh->cmd, 0);
		    } break;
		    case CMD_CACHE_GET_STATUS: {
		        if (mt->is_state_can_read()) {
			    //unsigned user_cnt = mt->ptr_list.size();
			    unsigned key_cnt = mt->key_count();
			    unsigned max_simultaneous_read = mt->max_simultaneous_read;
			    unsigned max_simultaneous_write = mt->max_simultaneous_write;
			    unsigned buf[10];
			    buf[0] = key_cnt;
			    buf[1] = max_simultaneous_read;
			    buf[2] = max_simultaneous_write;
			    //response(client_sock, rh->cmd, 0, (void*)&user_cnt, (unsigned)sizeof(user_cnt));
			    response(client_sock, rh->cmd, 0, (void*)buf, sizeof(unsigned)*3);
			} else {
			    response(client_sock, rh->cmd, ERR_FORBID_BY_CUR_STATE);
			}
		    } break;
		    default: {
		        response(client_sock, rh->cmd, ERR_UNKNOWN_CMD);
		    } break;
		    }

		    free(buf);
	        }
	    }
            usleep(1000);
        }
        i_am_stoped = true;
    }
};


void main_thread::init() {
    int ret = 0; 
    //ui.init();
    ht.init(65536, 16384, 30020, 30011);

    unsigned short cmd_port = 0;
    unsigned short data_port = 0;

        cmd_port = 9527;
    data_port = 9527;

    printf("cmdport=%d,dataport=%d\n", cmd_port, data_port);

    cmd_thread* ct = new cmd_thread;

    ret = create_tcp_server("127.0.0.1", cmd_port);
    if (ret < 0) {
        printf("create cmd socket failed! errno:%d emsg:%s\n", errno, strerror(errno));
        _exit(1);
    }
    ct->cmd_ltn_sock = ret;

    ret = create_tcp_server("127.0.0.1", data_port);
    if (ret < 0) {
        printf("create data socket failed! errno:%d emsg:%s\n", errno, strerror(errno));
        _exit(1);
    }
    data_ltn_sock = ret;

    ct->mt = this;
    for (int i = 0; i < thread_count; ++i) {
        data_thread* dt = new data_thread;
	dt->mt = this;
        worker_thread* wt = dt;
        wtl.push_back(wt);
    }

    vector<worker_thread*> tmp;
    tmp.push_back(ct);
    assure_start_worker_thread(tmp);

    assure_start_worker_thread(wtl);
}

/*
void main_thread::rebuild_index() {
    clear_the_whole_blocks();

    for (unsigned i = 0; i < wtl.size(); ++i) {
        data_thread* dt = (data_thread*)wtl[i];
	for (unsigned j = 0; j < dt->bl.size(); ++j) {
	    block b = dt->bl[j];
	    bl.push_back(b);
	    list_iterator li((unsigned*)b.frd,b.offset/sizeof(unsigned));
            unsigned uin;
	    unsigned len;
	    unsigned * data = li.next(uin, len);
	    while (data) {
	        //printf("uin:%u len:%u\n", uin, len);
		//ptr_list.push_back(data-2);
		//ui.add_just_add(uin, ptr_list.size()-1);
	        data = li.next(uin, len);
	    }
	}
	dt->bl.clear();
    }

    timeval s,e;
    gettimeofday(&s,NULL);
    update_the_sort_of_ui();
    gettimeofday(&e,NULL);
    printf("update_the_sort_of_ui, elapse:%dms\n", e.tv_sec*1000+e.tv_usec/1000-(s.tv_sec*1000+s.tv_usec/1000));
    //ui.dump();
}
*/

bool operator < (const uin_index::term& l, const uin_index::term& r) {
    return (l.uin > r.uin);
}
/*
void main_thread::update_the_sort_of_ui() {
    
    wb::min_heap<uin_index::term> mh;
    for (unsigned i = 0; i < ui.t.size(); ++i) {
        vector<uin_index::term>& range = ui.t[i];
	if (range.size() == 0)
	    continue;

	mh.init(range.size());

	for (unsigned k = 0; k < range.size(); ++k) {
	    mh.add_element(range[k]);
	}

	mh.sort();

	uin_index::term* ary = mh.array();
	assert(mh.size() == range.size());
	for (unsigned k = 0; k < mh.size(); ++k) {
	    range[k] = ary[k];
	}

	mh.free();
    }
}
*/


void main_thread::get_pending_blocks()
{
    for (unsigned i = 0; i < wtl.size(); ++i) {
        data_thread* dt = (data_thread*)wtl[i];
	dt->main_get_data(waiting_bl);
    }
}

void main_thread::process_one_block(block& b)
{
    list_iterator li((unsigned char*)b.frd, b.offset);
    unsigned uin;
    unsigned bytes;
    unsigned len;

    unsigned char* frd = li.next(uin, bytes);
    while (frd) {
        void* data = (frd-2*sizeof(unsigned));

	add_user(uin, bytes+2*sizeof(unsigned), (char*)data);

        //printf("add user uin:%u bytes:%u frd:%u\n", uin, bytes, bytes/sizeof(unsigned));
	frd = li.next(uin, bytes);
    }
}

void main_thread::read_block()
{
    get_pending_blocks();
	
    if (waiting_bl.size()) {
        vector<block>::iterator lastone = waiting_bl.begin()+(waiting_bl.size()-1);
        process_one_block(*lastone);
        lastone->free_memory();
        waiting_bl.erase(lastone);
    }
}

void main_thread::run() {
    init();
   
    printf("main_thread::run\n");
    while (1) {
        mt_lock.lock();

	if (!is_state_can_read()) {
	    read_block();
	    if (waiting_bl.size() > 0) {
	        mt_lock.unlock();
	        continue;
	    }
	}

	mt_lock.unlock();

        usleep(1000);
    }
}




int mt_cache_run(int port, int thread_count)
{
    main_thread* mt = new main_thread;
    mt->thread_count = thread_count;
    mt->run();
    return 0;
}
