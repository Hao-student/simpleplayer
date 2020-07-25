#pragma once

#include <vector>
#include <mutex> 

template<typename T>
// �̰߳�ȫ�Ķ���
class Queue
{
public:
	Queue() {
		q.clear();
	}
	// push ����
	void push(T val) {
		m.lock();
		q.push_back(val);
		m.unlock();
	}
	// pull ���У�����false˵������Ϊ��
	bool pull(T& val) {
		m.lock();
		if (q.empty()) {
			m.unlock();
			return false;
		}
		else {
			val = q.front();
			q.erase(q.begin());
			m.unlock();
			return true;
		}
	}
	// empty ���ض����Ƿ�Ϊ��
	bool empty() {
		m.lock();
		bool isEmpty = q.empty();
		m.unlock();
		return isEmpty;
	}
	// size ���ض��еĴ�С
	int size() {
		m.lock();
		int s = q.size();
		m.unlock();
		return s;
	}
protected:
	std::vector<T> q; // ����
	std::mutex     m; // ��
};