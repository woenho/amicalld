```
/*
* programming. woenho@callgate.co.kr
* update. 2022-02-07
*/
```

dtmf 서버기능을 유지한 채 콜센타 서버 역할을 하도록 구성한다<br/>
calld 서버는 각 상담전화번호를 queue 그룹으로 묶어서 관리한다<br/>
상담원 내부전화번호는 Asterisk에 연결되어 있는 것만 유효하다<br/>
queue는 다음데이타 구조를 가진다<br/>
<br/>
class CAgent {<br/>
	string agnet_id;	// [agent_id](key) -> 상담원고유번호, phone 상담원의 내부전화번호<br/>
	string agent_pw;<br/>
	string agent_name;<br/>
	string phone;		// 상담사가 전화를 받을 내부 전화번호<br/>
	string queue_id;	// 해당 상담사가 소속되어 있는 큐 고유번호<br/>
	char conteccted_status; // 상담사가 현재 연결되어 있는가?<br/>
}<br/>
class CCallee {<br/>
	string callee_id;	// [callee_id](key) -> 'calld_' + callee_num (큐번호 혹은 상담번호)<br/>
	string callee;		// 상담전화번호 (agent 가 가진 내부전화번호가 아님)<br/>
	string queue_id;	// 이 상담전화번호는 어느 큐에 소속되어 있는가? 큐 클래스의 m_agent 들 중 하나가 수신할 수 있다.<br/>
}<br/>
class CHistory {<br/>
	string caller;		// 발신자 번호, [caller + begin] (key)<br/>
	time begin;			// 전화가 걸려온 시각, [caller + begin] (key)<br/>
	string agent_id;	// agent_id -> 전화 수신한 상담자고유번호<br/>
	time start;			// 통화 시작시각 (값이 널이면 부재중 통화)<br/>
	time end;			// 통화 종료시각<br/>
	string channel;		// 해당 전화통화의 채널명<br/>
	string callee;		// 상담전화번호<br/>
	string record;		// 통화녹음파일 경로<br/>
	string memo;		// 기록된 상담내역<br/>
}<br/>
class CQueue {<br/>
	string queue_id;	// [queue_id] (key)<br/>
	map<const char*, CAgent*> m_agent;<br/>
	map<const char*, CCallee*> m_callee;<br/>
	map<const char*, CHistory*> m_history;<br/>
}<br/>
<br/>



