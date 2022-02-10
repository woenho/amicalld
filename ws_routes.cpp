#include "websocket.h"
#include "util.h"
#include "amiproc.h"
#include "amicalld.h"

extern int log_event_level;

int g_processmon_sd = 0;

TST_STAT websocket_basecheck(PTST_SOCKET psocket)
{
	if (!psocket || !psocket->user_data)
		return tst_disconnect;

	WS_INFO& wsinfo = *(PWS_INFO)psocket->user_data->s;

	if (wsinfo.is_ping) {
		TRACE("--ws- recv ping opcode\n");
		ws_writepong(psocket);
		return tst_suspend;
	}

	if (wsinfo.is_pong) {
		TRACE("--ws- recv pong opcode\n");
		return tst_suspend;
	}

	if (wsinfo.is_close) {
		TRACE("--ws- recv disconnect opcode\n");
		return tst_disconnect;
	}

	if (wsinfo.is_bin) {
		TRACE("--ws- recv binary opcode\n");
		// 아직 지원하지 않는다
		ws_writetext(psocket, "opcode binary는 지원하지 않는다");
		return tst_suspend;
	}

	if (!wsinfo.is_text) {
		// 프로토콜 오류
		ws_writetext(psocket, "opcode 지정이 잘못 되었습니다.");
		return tst_disconnect;
	}

	TRACE("--ws- recv text opcode, len=%lu:%s:\n", wsinfo.data_len, wsinfo.data);

	// 들어온 데이타는 text 이다...
	if (wsinfo.data_len < 1) {
		// 프로토콜 오류
		ws_writetext(psocket, "text의 길이가 0입니다.");
		return tst_suspend;
	}

	return tst_run;
}

TST_STAT websocket_alive(PTST_SOCKET psocket)
{
	TST_STAT next = websocket_basecheck(psocket);
	if (next != tst_run)
		return next;

	WS_INFO& wsinfo = *(PWS_INFO)psocket->user_data->s;

	CQueryString qs(wsinfo.data, CQueryString::ALL);
	if (*qs.Get("id") && *qs.Get("check")) {
		const char* id = qs.Get("id");
		const char* check = qs.Get("check");
		int id_len = strlen(id);
		int check_len = strlen(check);
		if (id_len < 5 || check_len < 5 || id[1] != check[4] || id[3] != check[0]) {
			ws_writetext(psocket, "{\"code\": 400, \"msg\":\"keepalive check error... will be disconnected\"}");
			return tst_disconnect;
		}
		if (g_processmon_sd && g_processmon_sd != psocket->sd) {
			// 이미 등록된 프로세스모니터가 있다면 유효한지 확인한다.
			map<int, PTST_SOCKET>::iterator it = server.m_connect.find(g_processmon_sd);
			if (it != server.m_connect.end()) {
				ws_writetext(psocket, "{\"code\": 409, \"msg\":\"I have another processmon's connection.\"}");
				return tst_disconnect;
			}
		}
		ws_writetext(psocket, "{\"code\": 201, \"msg\":\"keepalive check ok.\"}");
		g_processmon_sd = psocket->sd;
	} else {
		ws_writetext(psocket, "{\"code\": 401, \"msg\":\"keepalive check error... will be disconnected\"}");
		return tst_disconnect;
	}


	return tst_suspend;
}

TST_STAT process_cmd(PTST_SOCKET psocket, const char* cmd, const char* param)
{
	// WS_INFO& wsinfo = *(PWS_INFO)psocket->user_data->s;

	if (!strcmp(cmd, "file_config_reload")) {
		if (reload_queue(param)) {
			ws_writetext(psocket, "{\"code\": 200, \"msg\":\"config reload ok.\"}");
		}else {
			ws_writetext(psocket, "{\"code\": 501, \"msg\":\"config reload faled.\"}");
		}
	} else if (!strcmp(cmd, "info")) {
		if (!strcmp(param, "processmon_sd")) {
			ws_writetext(psocket, "{\"code\": 200, \"msg\":\"g_processmon_sd=%d\"}", g_processmon_sd);
		}
	} else {
		ws_writetext(psocket, "{\"code\": 400, \"msg\":\"known cmd\"}");
	}
	return tst_suspend;
}

TST_STAT websocket_admin(PTST_SOCKET psocket)
{
	TST_STAT next = websocket_basecheck(psocket);
	if (next != tst_run)
		return next;

	WS_INFO& wsinfo = *(PWS_INFO)psocket->user_data->s;

	CQueryString qs(wsinfo.data, CQueryString::ALL);
	if (*qs.Get("id") && *qs.Get("check")) {
		const char* id = qs.Get("id");
		const char* check = qs.Get("check");
		int id_len = strlen(id);
		int check_len = strlen(check);
		if (id_len < 5 || check_len < 5 || id[1] != check[4] || id[3] != check[0]) {
			ws_writetext(psocket, "{\"code\": 400, \"msg\":\"admin id check error... will be disconnected\"}");
			return tst_disconnect;
		}
		
		if (*qs.Get("cmd")) {
			return process_cmd(psocket, qs.Get("cmd"), qs.Get("param"));
		}
	}
	else {
		ws_writetext(psocket, "{\"code\": 401, \"msg\":\"admin id check error... will be disconnected\"}");
		return tst_disconnect;
	}


	return tst_suspend;
}

TST_STAT websocket_login(PTST_SOCKET psocket)
{
	TST_STAT next = websocket_basecheck(psocket);
	if (next != tst_run)
		return next;

	WS_INFO& wsinfo = *(PWS_INFO)psocket->user_data->s;

	CQueryString qs(wsinfo.data, CQueryString::ALL);
	if (*qs.Get("id") && *qs.Get("pw")) {
		const char* id = qs.Get("id");
		const char* pw = qs.Get("pw");
		map<string, PAgent>::iterator iter = g_pagent->find(id);
		if (iter == g_pagent->end()) {
			ws_writetext(psocket, "{\"code\": 401, \"msg\":\"login id/pw check error... will be disconnected\"}");
			return tst_disconnect;
		}
		if (strcmp(iter->second->agent_pw, pw)) {
			ws_writetext(psocket, "{\"code\": 402, \"msg\":\"login id/pw check error... will be disconnected\"}");
			return tst_disconnect;
		}
		ws_writetext(psocket, "{\"code\": 200, \"msg\":\"login ok\"}");
		wsinfo.websocket_func = websocket_call;
		wsinfo.agent = iter->second;
		wsinfo.agent->status = 1;
		
		/// --- 최초 연결시 해야할 많은 일들이 있다.
		// 일단 클라이언트의 자바스크립트에서 요청하는 자료만 보내주기로 한다.

		return tst_suspend;
	}
	else {
		ws_writetext(psocket, "{\"code\": 403, \"msg\":\"login id/pw check error... will be disconnected\"}");
		return tst_disconnect;
	}


	return tst_suspend;
}


TST_STAT websocket_call(PTST_SOCKET psocket)
{
	TST_STAT next = websocket_basecheck(psocket);
	if (next != tst_run)
		return next;

	WS_INFO& wsinfo = *(PWS_INFO)psocket->user_data->s;

	conpt("%s(%s) called...", __func__, wsinfo.agent->agnet_id);

	// json 형식으로 로그인 초기에 필요한 모든 자료들을 순차적으로 클라이언트의 요청에 따라 내려 보내 준다
	// 1. 연결된 큐 정보 리스트업
	// 2. 각 큐별 히스토리
	// 3. 부재중 목록 작성
	// 4. 클라이언트 화면 구성











	return tst_suspend;
}




