
#ifndef _AMI_CALL_DAEMON_H_
#define _AMI_CALL_DAEMON_H_

#include "util.h"
#include "websocket.h"
#include "fileconfig.h"

int main(int argc, char* argv[]);

typedef struct CAget_t {
	char agnet_id[32];		// [agent_id](key) -> 상담원고유번호, phone 상담원의 내부전화번호<br/>
	char agent_pw[32];		// 비대칭암호화기법으로 저장
	char agent_name[64];	// 상담사이름
	char phone[32];			// 상담사가 전화를 받을 내부 전화번호<br/>
	list<string> queuelist;	// 해당 상담사가 소속되어 있는 큐 리스트<br/>
	char status;			// 상담사가 현재 연결되어 있는가? (0:비로그인, 1:대기 중, 2, 상담 중, 3: 상담불가, 4:전화미연결)
	char reload;			// 환경정보 리로드시 배제된 에이전트안가?
	CAget_t() { reset(); }
	~CAget_t() { reset(); }
	void reset() {
		bzero(agnet_id, sizeof(agnet_id));
		bzero(agent_pw, sizeof(agent_pw));
		bzero(agent_name, sizeof(agent_name));
		bzero(phone, sizeof(phone));
		queuelist.clear();
		status = '\0';
	}
	void setqueuelist(const char* list);
}CAgent, *PAgent;

typedef struct CHistory_t {
	// [begin + caller] desending key
	char caller[32];		// 발신자 번호, [begin + caller](key)<br/>
	time_t begin;			// 전화가 걸려온 시각, [begin + caller](key)<br/>
	char queue[32];			// 상담전화번호<br/>
	char agent_id[32];		// agent_id -> 전화 수신한 상담자고유번호<br/>
	time_t start;			// 통화 시작시각 (값이 널이면 부재중 통화)<br/>
	time_t end;				// 통화 종료시각<br/>
	time_t abandon;			// 부재중통화 삭제 시각
	char channel[64];		// 해당 전화통화의 채널명<br/>
	char uuid[32];			// uuid
	char record[512];		// 통화녹음파일 경로<br/>
	char* memo;				// 기록된 상담내역<br/>
	CHistory_t() { bzero(this, sizeof(*this)); }
	~CHistory_t() { if (memo) free(memo); }
}CHistory, *PHistory;

typedef struct CQueue_t {
	char queue_id[32];		// [queue_id](key) 상담전화번호 (agent 가 가진 내부전화번호가 아님)<br/>
	char queue_name[32];	// [queue_name]<br/>
	bool playment;		// 이 전화에 안내멘트를 플레이 하는가?
	bool recording;		// 이 전화는 통화를 녹음하는가?
	char queueing;		// 상agent에게 순차작으로 연결하는지 모든 agemt에 동시 벨을 울리는지? (0: all, 1: inline )
	map<string, PAgent> m_agent;		// 여기서는 포인트만 관리한다. 메모리해제금지
	map<string, PHistory> m_history;	// 여기서는 포인트만 관리한다. 메모리해제금지
	CQueue_t() { 
		bzero(queue_id, sizeof(queue_id));
		bzero(queue_name, sizeof(queue_name));
		m_agent.clear();
		m_history.clear();
	}
	~CQueue_t() {
		m_agent.clear();
		m_history.clear();
	}
}CQueue, *PQueue;

// 키가 포인터임에도 std:map<> 을 사용하는것은 향 후 소트할 필요가 있기 때문이다.
extern map<string, PQueue>* g_pqueue; 
extern map<string, PAgent>* g_pagent;
extern map<string, PHistory, std::greater<string> > g_history;

extern char g_queue_path[512];	// queue & agent 환경파일이 존재하는 폴더경로
extern bool reload_queue(const char* path = NULL);

#endif //_AMI_CALL_DAEMON_H_
