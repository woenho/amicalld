#include "amiaction.h"
#include "http.h"
#include "websocket.h"
#include "processevents.h"
#include "amicalld.h"

extern char amicalldCompileDate[20];

map<const char*, PQueue>* g_pqueue = new map<const char*, PQueue>;
map<const char*, PAgent>* g_pagent = new map<const char*, PAgent>;
map<const char*, PHistory> g_history;

char g_queue_path[512] = { 0 };	// queue & agent 환경파일이 존재하는 폴더경로

char cfg_path[512] = { 0 };

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
		conpt("--- disconnected client socket....%s(%s:%d)", __func__, inet_ntoa(psocket->client.sin_addr), ntohs(psocket->client.sin_port));
#endif
		if (psocket->type == sock_websocket) {
			TRACE("--ws- websocket session이 해제되었다.\n");
			
			// websocket 해제 시 keepalive 중단 처리필요함



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

void remove_queue(map<const char*, PQueue>& r_queue)
{
	map<const char*, PQueue>::iterator it_queue;

	for (it_queue = r_queue.begin(); it_queue != r_queue.end(); it_queue++) {
		delete it_queue->second;
	}
	r_queue.clear();
}

int add_queue(map<const char*, PQueue>& r_queue, CKeyset& ks)
{
	if (!*ks.getvalue("queue_id"))
		return -9;

	PQueue pqueue = new CQueue;
	strncpy(pqueue->queue_id, ks.getvalue("queue_id"), sizeof(pqueue->queue_id) - 1);
	
	if (*ks.getvalue("playment")) pqueue->playment = true;
	if (*ks.getvalue("recording")) pqueue->recording = true;
	pqueue->queueing = !strcmp(ks.getvalue("queueing"), "inline");


	map<const char*, PQueue>::iterator it_queue = r_queue.find(pqueue->queue_id);
	if (it_queue == r_queue.end()) {
		r_queue[pqueue->queue_id] = pqueue;
	}
	else {
		delete pqueue;
		return -1;
	}

	return 0;
}

void CAgent::setquelist(const char* list)
{
	char* queueid_list = strdup(list);
	char* next = queueid_list;
	char* queue;
	while (*next) {
		queue = next;
		next = strpbrk(next, ",;|");  // {',',';','|'} 3개 중 아무거난 구분자로 사용할 수 있다.
		if (!next) {
			// last token
			rtrim(queue);
			if (queue && *queue) {
				queuelist.push_back(strdup(queue));
			}
			break;
		}
		*next++ = '\0';
		rtrim(queue);
		if (queue && *queue) {
			queuelist.push_back(strdup(queue));
		}
		while (*next && *next == ' ')
			next++;
		queue = next;
	}
	free(queueid_list);
}

bool reload_queue()
{
	const char* err1 = "콜센타환경파일 지정오류";
	char szPath[512] = { 0, };
	int i = 0;

	map<const char*, PQueue>* l_pqueue = new map<const char*, PQueue>;
	map<const char*, PQueue>& l_queue = *l_pqueue;
	map<const char*, PAgent>* l_pagent = new map<const char*, PAgent>;
	map<const char*, PAgent>& l_agent = *l_pagent;
	map<const char*, PQueue>::iterator it_queue;
	map<const char*, PAgent>::iterator it_agent;
	map<const char*, PHistory>::iterator it_history;
	PQueue pqueue;
	PAgent pagent;
	PHistory phistory;

	CKeyset* pks;
	map<const char*, CKeyset*>::iterator it_ks;
	list<char*>::iterator it_list;

	CFileConfig fileQueue, fileAgent;

	int rc;

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
		conpt("queue id: %s", it_ks->first);
		for (i = 0; i < pks->getcount(); i++) {
			conpt("\t%s=%s", pks->getkey(i), pks->getvalue(i));
		}
#endif
		pqueue = new CQueue;
		strncpy(pqueue->queue_id, pks->getks(), sizeof(pqueue->queue_id) - 1);
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

	for (it_ks = fileAgent.m_config.begin(); it_ks != fileAgent.m_config.end(); it_ks++) {
		pks = it_ks->second;
#ifdef DEBUG
		conpt("agent id: %s", it_ks->first);
		for (i = 0; i < pks->getcount(); i++) {
			conpt("\t%s=%s", pks->getkey(i), pks->getvalue(i));
		}
#endif
		pagent = new CAgent;
		strncpy(pagent->agnet_id, pks->getks(), sizeof(pagent->agnet_id) - 1);
		strncpy(pagent->agent_pw, pks->getvalue("password"), sizeof(pagent->agent_pw) - 1);
		strncpy(pagent->agent_name, pks->getvalue("agent_name"), sizeof(pagent->agent_name) - 1);
		strncpy(pagent->phone, pks->getvalue("phone"), sizeof(pagent->phone) - 1);
		pagent->setquelist(pks->getvalue("queue"));
		
		for (it_list = pagent->queuelist.begin(); it_list != pagent->queuelist.end(); it_list++) {
			// char*를 l_queue.find(*it_list) 찾으면 안된다
			for (it_queue = l_queue.begin(); it_queue != l_queue.end(); it_queue++) {
				if (!strcmp(*it_list, it_queue->first)) {
					it_queue->second->m_agent[pagent->agnet_id] = pagent;
					conpt("add agent, queue=%s, agent=(%s:%s)", it_queue->first , pagent->agnet_id, pagent->agent_name);
					break;
				}
			}
			if (it_queue == l_queue.end()) {
				conpt("등록되지 않은 큐에 qgent를 할당하려고 했다.환경파일 오류처리 (%s)", *it_list);
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
		// char*를 l_queue.find(phistory->queue) 찾으면 안된다
		for (it_queue = l_queue.begin(); it_queue != l_queue.end(); it_queue++) {
			if (!strcmp(phistory->queue, it_queue->first)) {
				l_queue.find(phistory->agent_id)->second->m_history[phistory->histoyr_key] = phistory;
				break;
			}
		}
	}
	// 2. 구 에이전트 중 신 에이전트에 없는 에이전트는 접속 종료한다 (상담중인 에이전트는?)
	for (it_agent = g_pagent->begin(); it_agent != g_pagent->end(); it_agent++) {
		pagent = it_agent->second;
		map<const char*, PAgent>::iterator iter;
		// char*를 l_agent.find(pagent->agnet_id) 찾으면 안된다
		for (iter = l_agent.begin(); iter != l_agent.end(); iter++) {
			if (!strcmp(pagent->agnet_id, iter->first)) {
				// 이번에도 사용중으로 찾았다 
				break;
			}
		}
		if (it_agent == l_agent.end()) {
#ifdef DEBUG
			// logging

#endif
			// 구 에이전트가 새 에이전트 목록에서 삭제되었다.
			// 접속 종료 처리......
			// 상담중이면????
			// 향후 추가 코딩키로....


		}
	}
	// 3. 새 정보를 구 정보로 대체한다
	map<const char*, PQueue>* g_pqueue_old = g_pqueue;
	map<const char*, PAgent>* g_pagent_old = g_pagent;
	g_pqueue = l_pqueue;
	g_pagent = l_pagent;
	delete(g_pqueue_old);
	delete(g_pagent_old);





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









	// -----------------------------------------------------
	// ------ agent config loading -------------------------
	// -----------------------------------------------------



	return true;
}
