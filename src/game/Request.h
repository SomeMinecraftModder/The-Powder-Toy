#ifndef REQUEST_H
#define REQUEST_H

#include <map>
#include "curl/system.h"
#include <mutex>
#include <condition_variable>
#include <string>

// minor hacks so I can avoid including curl in the header file. This causes problems with mingw
typedef void CURL;
struct curl_slist;
struct curl_mime_s;
struct curl_httppost;
typedef struct curl_mime_s curl_mime;

class RequestManager;
class Request
{
	std::string uri;
	std::string response_body;

	CURL *easy;
	char *error_buffer;

	volatile curl_off_t rm_total;
	volatile curl_off_t rm_done;
	volatile bool rm_finished;
	volatile bool rm_canceled;
	volatile bool rm_started;
	std::mutex rm_mutex;

	bool added_to_multi;
	int status;

	struct curl_slist *headers;

	curl_mime *post_fields;
	curl_httppost *post_fields_first, *post_fields_last;
	std::map<std::string, std::string> post_fields_map;

	std::condition_variable done_cv;

	static size_t WriteDataHandler(char * ptr, size_t size, size_t count, void * userdata);

public:
	Request(std::string uri);
	virtual ~Request();

	void AddHeader(std::string name, std::string value);
	void AddPostData(std::map<std::string, std::string> data);
	void AuthHeaders(std::string ID, std::string session);

	void Start();
	std::string Finish(int *status);
	void Cancel();

	void CheckProgress(int *total, int *done);
	bool CheckDone();
	bool CheckCanceled();
	bool CheckStarted();

	friend class RequestManager;

	static std::string Simple(std::string uri, int *status, std::map<std::string, std::string> post_data = std::map<std::string, std::string>{});
	static std::string SimpleAuth(std::string uri, int *status, std::string ID, std::string session, std::map<std::string, std::string> post_data = std::map<std::string, std::string>{});

	static std::string GetStatusCodeDesc(int code);
};

extern const long timeout;
extern std::string proxy;
extern std::string user_agent;

#endif // REQUEST_H
