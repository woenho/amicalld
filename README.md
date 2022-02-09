```
/*
* programming. woenho@callgate.co.kr
* update. 2022-02-07
*/
```
<br/>
calld 서버는 간이 콜센타프로그램이다.<br/>
dtmf 서버기능을 유지한 채 콜센타 서버 역할을 하도록 구성한다<br/>
calld 서버는 각 상담전화번호를 queue 에 연결하여 관리한다<br/>
agent 내부전화번호는 Asterisk에 연결되어 있는 것만 유효하다<br/>
하나의 queue는 하나의 상담전화번호를 가진다.
하나의 agent는 여러 queue에 등록 될 수 있다.
처리환경은 환경파일에서 정보를 로딩하여 메모리에 상주시킨다.<br/>
동작중에 환경파일을 재로딩할 수 있다.<br/>
agent는 상담전화를 받으려면 웹소켓으로 calld서버에 로그인 하여야 한다.<br/>
<br/>
{1. 처리흐름} 수신된 전화가 [queue]에 등록된 전화번호이면 처리대상이 된다<br/>
1. 안내멘트 송출정보를 가져와 플레이한다 <외부서버에 http 송수신한다><br/>
2. 상담가능 상태의 agent 들 중 연결된 agent 에 전화를 연결한다<br/>
3. agent의 전화벨이 울리면 agent 컴퓨터의 웹화면에 websocket으로 상담정보를 알람한다.<br/>
4. agent가 전화를 받으면 agent 컴퓨터 웹화면에 상담내역 입력창을 기동한다.<br/>
5. 수신하지 못한 채 전화가 끊기면 부재중 전화목록에 들어간다<br/>
<br/>
{2. 상담화면구성}<br/>
1. 기본(등록된 queue별 상태목록): 벨이 울리고 있는 통화목록 + 상담중인 통화 목록 + 각 agent의 상태(상담가능,상담불가능,상담중,비연결)<br/>
2. 부재중(등록된 queue별): 수신하지 못한 통화목록 화면<br/>
3. History(등록된 queue별): 상담기록목록 화면 + 상세정보화면<br/>
<br/>
{3. 전체데이타 구조}<br/>
<br/>
[상담원]<br/>
class CAgent {<br/>
	string agnet_id;	// [agent_id](key) -> 상담원고유번호, phone 상담원의 내부전화번호<br/>
	string agent_pw;<br/>
	string agent_name;<br/>
	string phone;		// 상담사가 전화를 받을 내부 전화번호<br/>
	string queue_id;	// 해당 상담사가 소속되어 있는 큐 고유번호<br/>
	char conteccted_status; // 상담사가 현재 연결되어 있는가?<br/>
}<br/>
<br/>
[상담기록] 부재중 전화기록도 포함된다<br/>
class CHistory {<br/>
	string caller;		// 발신자 번호, [caller + begin](key)<br/>
	time begin;			// 전화가 걸려온 시각, [caller + begin](key)<br/>
	string callee;		// 상담전화번호<br/>
	string agent_id;	// agent_id -> 전화 수신한 상담자고유번호<br/>
	time start;			// 통화 시작시각 (값이 널이면 부재중 통화)<br/>
	time end;			// 통화 종료시각<br/>
	string channel;		// 해당 전화통화의 채널명<br/>
	string record;		// 통화녹음파일 경로<br/>
	string memo;		// 기록된 상담내역<br/>
}<br/>
<br/>
[상담전화번호목록]<br/>
class CQueue {<br/>
	string queue_id;	// [queue_id](key)<br/>
	map<const char*, CAgent*> m_agent;		// key: [agent_id] <br/>
	map<const char*, CHistory*> m_history;	// key: [caller + begin]<br/>
	string callee;		// 상담전화번호 (agent 가 가진 내부전화번호가 아님)<br/>
	bool playment;		// 이 전화에 안내멘트를 플레이 하는가?<br/>
	bool recording;		// 이 전화는 통화를 녹음하는가?<br/>
	string queueing;	// 상agent에게 순차작으로 연결하는지 모든 agemt에 동시 벨을 울리는지?<br/>
}<br/>
<br/>
{4. 환경파일 구조}<br/>
[123456]				// [queue_id]<br/>
name=상담전화_1			// [queue name]<br/>
playment=N				// 안내멘트플레이 여부<br/>
recording=Y				// 상담내역 녹취여부<br/>
queueing={inline,all}	// 상담사 연결방식<br/>
<br/>
[홍길동]					// 상담사 로그인 아이디<br/>
password = password1	// 상담사 로그인 암호<br/>
agent_name = 상담자_A1	// 상담사 신분<br/>
queue = {queue_id_1}, {queue_id_2}, {queue_id_3}	// 연결된 queue 목록<br/>
phone =	800020003		// 상담사 내부 전화번호<br/>
<br/>
{5.연동 라이브러리}<br/>
1. tstpool<br/>
2. AsyncThreadPool<br/>
3. amiutil<br/>
<br/>

개발노트<br/>
1. 환경정보 메모리 구조 개발<br/>
2. 환경파일 로딩 개발, 동작중 환경파일 재로딩 개발<br/>
3. 상담원 로그인 / 로그아웃 및 연동된 처리 개발<br/>
4. {기본 / 부재중 / 기록} 목록관리 시스템구성 개발<br/>
5. 수신전화 상담원 알람 개발<br/>
6. 통화연결 상담원 알람 개발<br/>
7. 상담내역 입력 및 기록 저장 개발<br/>
8. 부재중 처리 개발<br/>
9. 부재중 전화 콜백 처리 개발<br/>
10. 안내멘트처리(TTS) 연동서버 추가개발<br/>
11. webrtc를 통한 상담<br/>
<br/>


















