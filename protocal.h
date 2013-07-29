#ifndef CACHE_PROTOCAL_H
#define CACHE_PROTOCAL_H


enum Command
{
    CMD_CACHE_UPDATE_STATE_UPDATING = 1,
    CMD_CACHE_UPDATE_STATE_ESTABLISHED = 2,
    CMD_CACHE_UPDATE_STATE_APPENDING = 3,
    CMD_CACHE_GET_STATUE = 4,
    
    CMD_CACHE_ADD  = 101,
    CMD_CACHE_ADD_BATCH ,
    CMD_CACHE_GET ,
    CMD_CACHE_GET_BATCH ,
    
    CMD_CACHE_SHUTDOWN = 1001,
};

enum Error
{
    ERR_EMPTY_REQUEST = 10001,
    ERR_TOOLARGE_REQUEST ,
    ERR_INVALID_REQUEST_SIZE ,
    ERR_FORBID_BY_CUR_STATE ,
    ERR_OOM ,
    ERR_TIMEOUT,
    ERR_UNKNOWN_CMD ,
};

const int LIMIT_REQUEST_SIZE = 64*1024*1024;


class request_header
{
public:
    unsigned int length;
    Command cmd;
    
    bool encode(char *buf, unsigned int size, unsigned int& offset)
    {
        if (offset + sizoef(request_header) > size)
            return false;
        request_header *rh = (request_header*)(buf + offset);
        rh->length = length;
        rh->cmd = cmd;
        offset += sizeof(request_header);
        return true;
    }
    
    bool decode(char *buf, unsigned int size, unsigned int& offset)
    {
        if (offset + sizoef(request_header) > size)
            return false;
        request_header *rh = (request_header*)(buf + offset);
        this.length = rh->length;
        this.cmd = rh->cmd;
        offset += sizeof(request_header);
        return true;
    }
};

class response_header
{
public:
    unsigned int length;
	int err;
	int cmd;

	bool encode(char* buf, unsigned size, unsigned& offset) {
		if (offset+sizeof(response_header) > size)
			return false;
		response_header* rh = (response_header*)(buf+offset);
		rh->length = length;
        rh->err = err;
		rh->cmd = cmd;
		offset += sizeof(response_header);
		return true;
	}
	bool decode(char* buf, unsigned size, unsigned& offset) {
		if (offset+sizeof(response_header) > size)
			return false;

		response_header* rh = (response_header*)(buf+offset);
		length = rh->length;
        err = rh->err;
		cmd = rh->cmd;
		offset += sizeof(response_header);
		return true;
	}
};
#endif
