```
/*
* programming. woenho@callgate.co.kr
* update. 2022-02-07
*/
```

dtmf ��������� ������ ä �ݼ�Ÿ ���� ������ �ϵ��� �����Ѵ�<br/>
calld ������ �� �����ȭ��ȣ�� queue �׷����� ��� �����Ѵ�<br/>
���� ������ȭ��ȣ�� Asterisk�� ����Ǿ� �ִ� �͸� ��ȿ�ϴ�<br/>
queue�� ��������Ÿ ������ ������<br/>
<br/>
class CAgent {<br/>
	string agnet_id;	// [agent_id](key) -> ����������ȣ, phone ������ ������ȭ��ȣ<br/>
	string agent_pw;<br/>
	string agent_name;<br/>
	string phone;		// ���簡 ��ȭ�� ���� ���� ��ȭ��ȣ<br/>
	string queue_id;	// �ش� ���簡 �ҼӵǾ� �ִ� ť ������ȣ<br/>
	char conteccted_status; // ���簡 ���� ����Ǿ� �ִ°�?<br/>
}<br/>
class CCallee {<br/>
	string callee_id;	// [callee_id](key) -> 'calld_' + callee_num (ť��ȣ Ȥ�� ����ȣ)<br/>
	string callee;		// �����ȭ��ȣ (agent �� ���� ������ȭ��ȣ�� �ƴ�)<br/>
	string queue_id;	// �� �����ȭ��ȣ�� ��� ť�� �ҼӵǾ� �ִ°�? ť Ŭ������ m_agent �� �� �ϳ��� ������ �� �ִ�.<br/>
}<br/>
class CHistory {<br/>
	string caller;		// �߽��� ��ȣ, [caller + begin] (key)<br/>
	time begin;			// ��ȭ�� �ɷ��� �ð�, [caller + begin] (key)<br/>
	string agent_id;	// agent_id -> ��ȭ ������ ����ڰ�����ȣ<br/>
	time start;			// ��ȭ ���۽ð� (���� ���̸� ������ ��ȭ)<br/>
	time end;			// ��ȭ ����ð�<br/>
	string channel;		// �ش� ��ȭ��ȭ�� ä�θ�<br/>
	string callee;		// �����ȭ��ȣ<br/>
	string record;		// ��ȭ�������� ���<br/>
	string memo;		// ��ϵ� ��㳻��<br/>
}<br/>
class CQueue {<br/>
	string queue_id;	// [queue_id] (key)<br/>
	map<const char*, CAgent*> m_agent;<br/>
	map<const char*, CCallee*> m_callee;<br/>
	map<const char*, CHistory*> m_history;<br/>
}<br/>
<br/>



