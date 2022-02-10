#include "amiaction.h"
#include "http.h"
#include "websocket.h"
#include "processevents.h"
#include "amicalld.h"

extern char amicalldCompileDate[20];
extern int g_processmon_sd;

map<string, PQueue>* g_pqueue = new map<string, PQueue>;
map<string, PAgent>* g_pagent = new map<string, PAgent>;
map<string, PHistory, std::greater<string> > g_history;
map<string, PQueue>* g_pold_queue = NULL;
map<string, PAgent>* g_pold_agent = NULL;

char g_queue_path[512] = { 0 };	// queue & agent 환경파일이 존재하는 폴더경로

char cfg_path[512] = { 0 };


void CAgent::setqueuelist(const char* list)
{
	char* queueid_list = strdup(list);
	char* next = queueid_list;
	char* queue;
	while (*next) {
		queue = next;
		next = strpbrk(next, ",;|");  // {',',';','|'} 3개 중 아무거나 구분자로 사용할 수 있다.
		if (!next) {
			// last token
			rtrim(queue);
			if (queue && *queue) {
				queuelist.push_back(queue);
			}
			break;
		}
		*next++ = '\0';
		rtrim(queue);
		if (queue && *queue) {
			queuelist.push_back(queue);
		}
		while (*next && (*next == ' ' || *next == '\t'))
			next++;
	}
	free(queueid_list);
}

void sig_handler(int signo, siginfo_t* info, /*ucontext_t*/void* ucp)
{
	sigset_t sigset, oldset;

	// 핸들러가 수행되는 동안 수신되는 모든 시그널에 대해서 블럭한다.
	// 시그널 처리 중에 다른 시그널 처리가 실행되면 예상치 못한 문제 발생

	sigfillset(&sigset);
	// 시그널 블럭
	if (sigprocmask(SIG_BLOCK, &sigset, &oldset) < 0)
	{
		conpt("sigprocmask %d error", signo);
	}

	conpt("----- recv signal:%d, my_pid=%d, si_pid=%d", signo, getpid(), info->si_pid);

	if (signo == SIGINT) // kill -2
	{
		server.m_main_run = false;
	}
	else if (signo == SIGUSR2) // kill -12 어떤 시스템은 31, 시스템마다 다름
	{
		PTHREADINFO pThread = atp_getThreadInfo();
		int nCount = atp_getThreadCount();
		int nQueueRealtime = atp_getRealtimeQueueCount();
		int nQueueNormal = atp_getNormalQueueCount();
		int nIndx;

		conft("atp compile date: %s", atpCompileDate);
		conft("tst compile date: %s", tstCompileDate);
		conft("util compile date: %s", amiutilCompileDate);
		conft("calld compile date: %s", amicalldCompileDate);

		conft("atp thread Realtime queue count=%d", nQueueRealtime);
		conft("atp thread Normal queue count=%d", nQueueNormal);

		for (nIndx = 0; nIndx < nCount; nIndx++) {
			conft("atp threadno=%d, realtime execute=%lu average elapsed=%.3f, normal execute=%lu average elapsed=%.3f"
				, pThread[nIndx].nThreadNo
				, pThread[nIndx].nRealtimeCount
				, atp_getAverageRealtimeWorkingtime(nIndx) / 1e+3	// 평균 작업소요 시간(mili)
				, pThread[nIndx].nNormalCount
				, atp_getAverageNormalWorkingtime(nIndx) / 1e+3		// 평균 작업소요 시간(mili)
			);
		}
		// tstpoll
		for (nIndx = 0; nIndx < (int)server.m_thread_count; nIndx++) {
			conft("tst threadno=%d, execute=%lu average elapsed=%.3f, most_elapsed=%.3f"
				, server.m_workers[nIndx].thread_no
				, server.m_workers[nIndx].exec_count
				, server.m_workers[nIndx].get_averageElapsedtime() / 1e+3
				, server.m_workers[nIndx].most_elapsed / 1e+9
			);
		}
		CWebConfig cfg;
		if (cfg.Open(cfg_path) > 0) {
			log_event_level = atoi(cfg.Get("LOG","ev_level").c_str());
		}
		conft("log_event_level=%d", log_event_level);
		
		reload_queue();
	}
	else if (signo == SIGUSR1) // kill -10 어떤 시스템은 30, 시스템마다 다름
	{
		PTHREADINFO pThread = atp_getThreadInfo();
		int nCount = atp_getThreadCount();
		int nQueueRealtime = atp_getRealtimeQueueCount();
		int nQueueNormal = atp_getNormalQueueCount();
		int nIndx;

		conft("atp compile date: %s", atpCompileDate);
		conft("tst compile date: %s", tstCompileDate);
		conft("util compile date: %s", amiutilCompileDate);
		conft("calld compile date: %s", amicalldCompileDate);

		conft("atp thread Realtime queue count=%d", nQueueRealtime);
		conft("atp thread Normal queue count=%d", nQueueNormal);

		for (nIndx = 0; nIndx < nCount; nIndx++) {
			conft("atp threadno=%d, realtime execute=%lu average elapsed=%.3f, normal execute=%lu average elapsed=%.3f"
				, pThread[nIndx].nThreadNo
				, pThread[nIndx].nRealtimeCount
				, atp_getAverageRealtimeWorkingtime(nIndx) / 1e+3	// 평균 작업소요 시간(mili)
				, pThread[nIndx].nNormalCount
				, atp_getAverageNormalWorkingtime(nIndx) / 1e+3		// 평균 작업소요 시간(mili)
			);
			pThread[nIndx].nRealtimeCount = pThread[nIndx].nNormalCount = 0;	// excute count reset
		}

		// tstpoll
		for (nIndx = 0; nIndx < (int)server.m_thread_count; nIndx++) {
			conft("tst threadno=%d, execute=%lu average elapsed=%.3f, most_elapsed=%.3f"
				, server.m_workers[nIndx].thread_no
				, server.m_workers[nIndx].exec_count
				, server.m_workers[nIndx].get_averageElapsedtime() / 1e+3
				, server.m_workers[nIndx].most_elapsed / 1e+9
			);
			server.m_workers[nIndx].exec_count = server.m_workers[nIndx].idle_count = 0;
			server.m_workers[nIndx].sum_time = server.m_workers[nIndx].most_elapsed = 0;
		}

	}
	else if (signo == SIGSEGV)
	{
		//signal(signo, SIG_ERR);
		conpt("Caught SIGSEGV : si_pid->%d, si_code->%d, addr->%p", info->si_pid, info->si_code, info->si_addr);

		fprintf(stderr, "Fault address: %p\n", info->si_addr);
		switch (info->si_code)
		{
		case SEGV_MAPERR:
			fprintf(stderr, "Address not mapped.\n");
			break;

		case SEGV_ACCERR:
			fprintf(stderr, "Access to this address is not allowed.\n");
			break;

		default:
			fprintf(stderr, "Unknown reason.\n");
			break;
		}


		signal(signo, SIG_IGN);
	}

	// 시그널 언블럭
	if (sigprocmask(SIG_UNBLOCK, &sigset, &oldset) < 0)
	{
		conpt("sigprocmask %d error", signo);
	}

}

TST_STAT calld_disconnected(PTST_SOCKET psocket) {
	if (!psocket)
		return tst_suspend;

	if (psocket == ami_socket) {
		// if (psocket->type == sock_client && psocket->user_data->type == ami_base) {
			conpt("--- 흐미 ami 끊어졌다.....");
			ami_socket = NULL;
		// }
	} else {
#ifdef DEBUG
		conpt("--- disconnected client socket....sd=%d, type=%d (%s:%d)", psocket->sd, psocket->type, inet_ntoa(psocket->client.sin_addr), ntohs(psocket->client.sin_port));
#endif
		if (psocket->type == sock_websocket) {
			TRACE("--ws- websocket session이 해제되었다.\n");
			
			// websocket 해제 시 keepalive 중단 처리필요함
			if (g_processmon_sd == psocket->sd) {
				g_processmon_sd = 0;
				conpt("--- disconnected processmon socket....sd=%d (%s:%d)", psocket->sd, inet_ntoa(psocket->client.sin_addr), ntohs(psocket->client.sin_port));
			}

			// agent 가 접속 종료하면 그 상태처리를 한다.
			WS_INFO& wsinfo = *(PWS_INFO)psocket->user_data->s;
			PAgent pagent = wsinfo.agent;
			if (!pagent) {
				// 아직 로그인 하지 않아서 에이전트 정보가 설정되지 않았다.
				return tst_suspend;
			}

			// 로그인했던 에이전트는 로그아웃 시 처리할 것들이 있다.
			// 1. 로그인 상태 변환
			// 2. 다른 큐 및 로그인 사용자 화면에 해당 내역 반영

			pagent->status = 0;






		}
	}

	return tst_suspend;
}


int main(int argc, char* argv[])
{
	if (argc == 2 && !strcmp(argv[1], "-v")) {
		printf("\namicalld version 2.0\n\n");
		printf("atp compile date: %s\n", atpCompileDate);
		printf("tst compile date: %s\n", tstCompileDate);
		printf("util compile date: %s\n", amiutilCompileDate);
		printf("calld compile date: %s\n", amicalldCompileDate);
		return 0;
	}

	getcwd(cfg_path, sizeof(cfg_path));
#if defined(DEBUGTRACE)
	strcat(cfg_path, "/amicalld_t.conf");
#elif defined(DEBUG)
	strcat(cfg_path, "/amicalld_d.conf");
#else
	strcat(cfg_path, "/amicalld.conf");
#endif
	printf("config file path:%s:\n", cfg_path);
	if (g_cfg.Open(cfg_path) < 0) {
		char* errmsg = strerror(errno);
		fprintf(stderr, "\n-------------------\n%s open failed\n%s\n", cfg_path, errmsg);
		fflush(stderr);
		return -1;
	}

	char* logmethod = strdup(g_cfg.Get("LOG", "method", "logfile").c_str());
	printf("log method=%s\n", logmethod ? logmethod : "error");

	if (logmethod && !strcmp(logmethod, "logfile")) {
		con_logfile(g_cfg.Get("LOG", "logfile", "./amicalld.log").c_str());
	}
	else if (logmethod && !strcmp(logmethod, "server")) {
		con_logudp(g_cfg.Get("LOG", "host", "127.0.0.1").c_str(), atoi(g_cfg.Get("LOG", "port", "26000").c_str()));
	}
	else {
		fprintf(stderr, "\n-------------------\nLOG method setting error in cofig file(%s)\n", cfg_path);
		if (logmethod) free(logmethod);
		return -1;
	}
	if (logmethod) free(logmethod);

	strcpy(g_queue_path, g_cfg.Get("CALLD", "path", ".").c_str());
	
	if (!reload_queue())
		return -1;


#if defined(DEBUGTRACE)
	conft("\n------------------------\n trace debuging info CALLD server start. pid(%d)\n========================================", getpid());
#elif defined(DEBUG)
	conft("\n------------------------\n possible debuging CALLD server start. pid(%d)\n========================================", getpid());
#else
	conft("\n------------------------\n optimization CALLD server start. pid(%d)\n========================================", getpid());
#endif
	conft("atp compile date: %s", atpCompileDate);
	conft("tst compile date: %s", tstCompileDate);
	conft("util compile date: %s", amiutilCompileDate);
	conft("calld compile date: %s", amicalldCompileDate);

	if (signal_init(&sig_handler, true) < 0)
		return -1;

	log_event_level = atoi(g_cfg.Get("LOG", "ev_level").c_str());
	conft("log_event_level=%d", log_event_level);




	// first는 test용
	// server.setEventNewConnected(first);

	server.setEventDisonnected(my_disconnected);

	if (server.create(atoi(g_cfg.Get("HTTP", "thread", "5").c_str())
					, "0.0.0.0"
					, atoi(g_cfg.Get("HTTP", "port", "4060").c_str())
					, http
					, 4096
					, 4096) < 1) {
		conft("---쓰레드풀이 기동 되지 못했다....");
		return 0;
	}

	// 연결해제시 호출로 반드시 설정해야한다
	// ami socket 연결해제시에 전역변수인 ami_socket 초기화가 필요하다
	server.m_fdisconnected = calld_disconnected;
	// -------------------------------------------------------------------------------------

	atp_create(atoi(g_cfg.Get("THREAD", "count", "5").c_str()), atpfunc);

	// 처리할 이벤트를 등록 한다....

	g_process.clear();
	ADD_AMI_EVENT_PROCESS("UserEvent", event_userevent);
	ADD_AMI_EVENT_PROCESS("DialEnd", event_dialend);

	map<const char*, void*>::iterator it;
	map<const char*, const char*>::iterator it_name;
	for (it = g_process.begin(); it != g_process.end(); it++) {
		it_name = g_process_name.find(it->first);
		conft(":%s: -> %s(), address -> %lX", it->first, it_name == g_process_name.end() ? "" : it_name->second, ADDRESS(it->second));
	}

	g_route.clear();
	ADD_HTTP_EVENT_PROCESS("/dtmf", http_dtmf);
	ADD_HTTP_EVENT_PROCESS("/transfer", http_transfer);
	ADD_HTTP_EVENT_PROCESS("/keepalive", http_alive);
	for (it = g_route.begin(); it != g_route.end(); it++) {
		it_name = g_route_name.find(it->first);
		conft(":%s: -> %s(), address -> %lX", it->first, it_name == g_route_name.end() ? "" : it_name->second, ADDRESS(it->second));
	}

	g_websocket.clear();
	ADD_WS_EVENT_PROCESS("/alive", websocket_alive);
	ADD_WS_EVENT_PROCESS("/admin", websocket_admin);
	ADD_WS_EVENT_PROCESS("/login", websocket_login);
	for (it = g_websocket.begin(); it != g_websocket.end(); it++) {
		it_name = g_websocket_name.find(it->first);
		conft(":%s: -> %s(), address -> %lX", it->first, it_name == g_websocket_name.end() ? "" : it_name->second, ADDRESS(it->second));
	}

	// -------------------------------------------------------------------------------------

#if 0
	int i = 300;
	while (--i && server.m_main_run) {
#else
	while (server.m_main_run) {
#endif
		if (!ami_socket) {
			// 연결을 시도하는 작업 요청
			PATP_DATA atpdata = atp_alloc(sizeof(AMI_LOGIN));
			AMI_LOGIN& login = *(PAMI_LOGIN)&atpdata->s;
			strncpy(login.Host, g_cfg.Get("AMI", "host", "127.0.0.1").c_str(), sizeof(login.Host));
			login.Port = atoi(g_cfg.Get("AMI", "port", "5038").c_str());
			strncpy(login.Username, g_cfg.Get("AMI", "user", "call").c_str(), sizeof(login.Username));
			strncpy(login.Secret, g_cfg.Get("AMI", "secret", "call").c_str(), sizeof(login.Secret));
			atpdata->func = amiLogin;
			atp_addQueue(atpdata);
			conft("REQUEST ami login...\n");
		}
		sleep(3);
	}



	conft("---쓰레드풀을 종료합니다.....\n");

	server.destroy();

	atp_destroy(gracefully);

	return 0;
}

bool reload_queue(const char* path)
{
	const char* err1 = "콜센타환경파일 지정오류";
	char szPath[512] = { 0, };
	int i = 0;

	map<string, PQueue>* l_pqueue = new map<string, PQueue>;
	map<string, PQueue>& l_queue = *l_pqueue;
	map<string, PAgent>* l_pagent = new map<string, PAgent>;
	map<string, PAgent>& l_agent = *l_pagent;
	map<string, PQueue>::iterator it_queue;
	map<string, PAgent>::iterator it_agent;
	map<string, PHistory>::iterator it_history;
	PQueue pqueue;
	PAgent pagent;
	PHistory phistory;

	CKeyset* pks;
	map<string, CKeyset*>::iterator it_ks;
	list<string>::iterator it_list;

	CFileConfig fileQueue, fileAgent;

	int rc;

	if (path && *path) {
		strcpy(g_queue_path, path);
	}

	if (!*g_queue_path) {
		conpt(err1);
		delete(l_pqueue);
		delete(l_pagent);
		return false;
	}

	l_queue.clear();

	// -----------------------------------------------------
	// ------ queue config loading -------------------------
	// -----------------------------------------------------

	snprintf(szPath, sizeof(szPath) - 1, "%s/calld_queue.conf", g_queue_path);


	rc = fileQueue.load(szPath);
	if (rc < 0) {
		delete(l_pqueue);
		delete(l_pagent);
		return false;
	}

	for (it_ks = fileQueue.m_config.begin(); it_ks != fileQueue.m_config.end(); it_ks++) {
		pks = it_ks->second;
#ifdef DEBUG
		conpt("queue id: %s", it_ks->first.c_str());
		for (i = 0; i < pks->getcount(); i++) {
			conpt("\t%s=%s", pks->getkeyname(i), pks->getvalue(i));
		}
#endif
		pqueue = new CQueue;
		strncpy(pqueue->queue_id, pks->getks().c_str(), sizeof(pqueue->queue_id) - 1);
		strncpy(pqueue->queue_name, pks->getvalue("name"), sizeof(pqueue->queue_name) - 1);
		pqueue->playment = *pks->getvalue("playment") == 'Y';
		pqueue->recording = *pks->getvalue("recording") == 'Y';
		pqueue->queueing = strcmp(pks->getvalue("queueing"), "inline") == 0;
		l_queue[pqueue->queue_id] = pqueue;
	}
	
	// -----------------------------------------------------
	// ------ agent config loading -------------------------
	// -----------------------------------------------------

	snprintf(szPath, sizeof(szPath) - 1, "%s/calld_agent.conf", g_queue_path);
	rc = fileAgent.load(szPath);
	if (rc < 0) {
		delete(l_pqueue);
		delete(l_pagent);
		return false;
	}

	map<string, PAgent>::iterator iter;

	for (it_ks = fileAgent.m_config.begin(); it_ks != fileAgent.m_config.end(); it_ks++) {
		pks = it_ks->second;
#ifdef DEBUG
		conpt("agent id: %s", it_ks->first.c_str());
		for (i = 0; i < pks->getcount(); i++) {
			conpt("\t%s=%s", pks->getkeyname(i), pks->getvalue(i));
		}
#endif
		pagent = new CAgent;
		strncpy(pagent->agnet_id, pks->getks().c_str(), sizeof(pagent->agnet_id) - 1);
		strncpy(pagent->agent_pw, pks->getvalue("password"), sizeof(pagent->agent_pw) - 1);
		strncpy(pagent->agent_name, pks->getvalue("agent_name"), sizeof(pagent->agent_name) - 1);
		strncpy(pagent->phone, pks->getvalue("phone"), sizeof(pagent->phone) - 1);
		// phone 은 삼담사가 사용하는 내부전화번호이다 중복되어서는 안된다 검증필요하다
		for (iter = l_agent.begin(); iter != l_agent.end(); iter++) {
			if (!strcmp(pagent->phone, iter->second->phone)) {
				conpt("agent의 phone number duplicated.(agent1=%s, agen2=%s)",iter->second->agnet_id, pagent->agnet_id);
				delete(l_pqueue);
				delete(l_pagent);
				return false;
			}
		}

		pagent->setqueuelist(pks->getvalue("queue"));
		
		for (it_list = pagent->queuelist.begin(); it_list != pagent->queuelist.end(); it_list++) {
			it_queue = l_queue.find(*it_list);
			if (it_queue != l_queue.end()) {
				it_queue->second->m_agent[pagent->agnet_id] = pagent;
				conpt("add agent, queue=%s, agent=(%s:%s)", it_queue->first.c_str(), pagent->agnet_id, pagent->agent_name);
			} else {
				conpt("없는 큐에 agent를 할당하려고 했다.(req_queue=%s), agent=(%s:%s)", it_list->c_str(), pagent->agnet_id, pagent->agent_name);
				delete(l_pqueue);
				delete(l_pagent);
				return false;
			}
		}
		l_agent[pagent->agnet_id] = pagent;
	}


	// -----------------------------------------------------
	// ------ merge config & history -------------------------
	// -----------------------------------------------------

	// 전제조건: 
	// 1. 작성된 히스토리는 삭제하지 않는다
	// 2. 기존 연결된 에이전트 중 새로 로딩된 현경에 없는 에이전트는 접속 종료한다
	// 
	// senario
	// 1. 큐의 히스토리 map 을 복사한다
	for (it_history = g_history.begin(); it_history != g_history.end(); it_history++) {
		phistory = it_history->second;
		it_queue = l_queue.find(phistory->queue);
		if (it_queue != l_queue.end()) {
			map<string, PHistory>& me = it_queue->second->m_history;
			if (me.find(it_history->first) == me.end()) {
				me[it_history->first] = phistory;
			}
#ifdef DEBUG
			else {
				conft("history info dup. key=%s", it_history->first.c_str());
			}
#endif
			break;
		}
	}
	// 2. 구 에이전트 중 신 에이전트에 없는 에이전트는 접속 종료하지 않는다 (상담중인 에이전트는?)
	for (it_agent = g_pagent->begin(); it_agent != g_pagent->end(); it_agent++) {
		iter = l_agent.find(it_agent->second->agnet_id);
		if (iter != l_agent.end()) {
			// 이번에도 사용중으로 찾았다 
			break;
		} else {
			pagent = it_agent->second;
			pagent->reload = 1; // 삭제된 에이전트이다 

#ifdef DEBUG
			// logging

#endif
			// 구 에이전트가 새 에이전트 목록에서 삭제되었다.
			// 상담중이면 상담중일 수도 있다. 삭제하지 않는다
			// 향후 추가 코딩키로....
			// 관리자가 웹소켓으로 삭제 명령을 내릴 수 있다. (g_pold_queue,g_pold_agent 삭제)


		}
	}
	// 3. 새 정보를 구 정보로 대체한다
	if (g_pold_queue) delete(g_pold_queue);
	if (g_pold_agent) delete(g_pold_agent);
	g_pold_queue = g_pqueue;
	g_pold_agent = g_pagent;
	g_pqueue = l_pqueue;
	g_pagent = l_pagent;





#ifdef DEBUG
	conpt("\n-------------------------------------\nall queue info\n--------------------------------------");
	for (it_queue = g_pqueue->begin(); it_queue != g_pqueue->end(); it_queue++) {
		pqueue = it_queue->second;
		conpt("queue_id=%s", pqueue->queue_id);
		conpt("\tqueue_name=%s", pqueue->queue_name);
		conpt("\tplayment=%d", pqueue->playment);
		conpt("\trecording=%d", pqueue->recording);
		conpt("\tqueueing=%s", pqueue->queueing ? "inline" : "all");
		conpt("\tagent list:");
		for (it_agent = pqueue->m_agent.begin(); it_agent != pqueue->m_agent.end(); it_agent++) {
			conpt("\t\t(%s:%s)", it_agent->second->agnet_id, it_agent->second->agent_name );
		}
	}
#endif

	return true;
}
