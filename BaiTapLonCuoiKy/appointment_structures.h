#ifndef APPOINTMENT_STRUCTURES_H
#define APPOINTMENT_STRUCTURES_H

#include <string>
#include <ctime>
#include <stdexcept>
#include <algorithm>
#include <vector>
#include <memory>

using namespace std;

struct Appointment {
    string appointment_id;
    string patient_id;
    string doctor_id;
    time_t time;
    string status; 
    bool is_valid;
    Appointment(string aid, string pid, string did, time_t t, string s)
        : appointment_id(aid), patient_id(pid), doctor_id(did), time(t), status(s), is_valid(true) {};
};

template <typename TValue>
struct Hashmap {
private:
    struct ListNode {
        string key;
        TValue value;
        ListNode* next;
        ListNode(string k, TValue v, ListNode* n = nullptr) : key(k), value(v), next(n) {}
    };
    ListNode** table;
    int size;

    int GetHashCode(string key) {
        int hash = 0;
        for (char c : key) {
            hash = hash * 31 + c;
        }
        return hash % size;
    }

public:
    Hashmap(int s = 100) : size(s) {
        table = new ListNode * [size]();
    }

    void Insert(string key, TValue value) {
        int index = GetHashCode(key);
        ListNode* head = table[index];
        if (!head) {
            table[index] = new ListNode(key, value);
            return;
        }
        while (head) {
            if (head->key == key) {
                throw runtime_error("ID lịch hẹn trùng lặp: " + key);
            }
            if (!head->next) break;
            head = head->next;
        }
        head->next = new ListNode(key, value);
    }

    TValue* Find(string key) {
        int index = GetHashCode(key);
        ListNode* head = table[index];
        while (head) {
            if (head->key == key) return &head->value;
            head = head->next;
        }
        return nullptr;
    }

    void Remove(string key) {
        int index = GetHashCode(key);
        ListNode* head = table[index];
        ListNode* prev = nullptr;
        while (head) {
            if (head->key == key) {
                if (prev) prev->next = head->next;
                else table[index] = head->next;
                delete head;
                return;
            }
            prev = head;
            head = head->next;
        }
    }

    vector<TValue*> GetAllValues() {
        vector<TValue*> result;
        for (int i = 0; i < size; i++) {
            ListNode* head = table[i];
            while (head) {
                result.push_back(&head->value);
                head = head->next;
            }
        }
        return result;
    }

    ~Hashmap() {
        for (int i = 0; i < size; i++) {
            ListNode* head = table[i];
            while (head) {
                ListNode* temp = head;
                head = head->next;
                delete temp;
            }
        }
        delete[] table;
    }
};

struct AVLNode {
    shared_ptr<Appointment> appointment;
    int height;
    AVLNode* left;
    AVLNode* right;
    AVLNode(shared_ptr<Appointment> app) : appointment(app), height(1), left(nullptr), right(nullptr) {}
};

struct AVLTree {
private:
    AVLNode* root;

    int Height(AVLNode* node) {
        return node ? node->height : 0;
    }

    int BalanceFactor(AVLNode* node) {
        return node ? Height(node->left) - Height(node->right) : 0;
    }

    void UpdateHeight(AVLNode* node) {
        if (node) {
            node->height = max(Height(node->left), Height(node->right)) + 1;
        }
    }

    AVLNode* RotateRight(AVLNode* y) {
        AVLNode* x = y->left;
        y->left = x->right;
        x->right = y;
        UpdateHeight(y);
        UpdateHeight(x);
        return x;
    }

    AVLNode* RotateLeft(AVLNode* x) {
        AVLNode* y = x->right;
        x->right = y->left;
        y->left = x;
        UpdateHeight(x);
        UpdateHeight(y);
        return y;
    }

    AVLNode* Insert(AVLNode* node, shared_ptr<Appointment> app) {
        if (!node) return new AVLNode(app);
        if (app->time < node->appointment->time)
            node->left = Insert(node->left, app);
        else if (app->time > node->appointment->time)
            node->right = Insert(node->right, app);
        else if (app->patient_id == node->appointment->patient_id &&
            app->doctor_id == node->appointment->doctor_id)
            throw runtime_error("Xung đột thời gian lịch hẹn cho cùng bệnh nhân và bác sĩ");
        else
            node->right = Insert(node->right, app);

        UpdateHeight(node);
        int balance = BalanceFactor(node);

        if (balance > 1 && app->time < node->left->appointment->time)
            return RotateRight(node);
        if (balance < -1 && app->time > node->right->appointment->time)
            return RotateLeft(node);
        if (balance > 1 && app->time > node->left->appointment->time) {
            node->left = RotateLeft(node->left);
            return RotateRight(node);
        }
        if (balance < -1 && app->time < node->right->appointment->time) {
            node->right = RotateRight(node->right);
            return RotateLeft(node);
        }
        return node;
    }

    AVLNode* FindMin(AVLNode* node) {
        while (node && node->left) node = node->left;
        return node;
    }

    AVLNode* Remove(AVLNode* node, string aid) {
        if (!node) return nullptr;

        if (node->appointment->appointment_id == aid) {
            if (!node->left) {
                AVLNode* temp = node->right;
                delete node;
                return temp;
            }
            if (!node->right) {
                AVLNode* temp = node->left;
                delete node;
                return temp;
            }
            AVLNode* minNode = FindMin(node->right);
            node->appointment = minNode->appointment;
            node->right = Remove(node->right, minNode->appointment->appointment_id);
        }
        else if (aid < node->appointment->appointment_id) {
            node->left = Remove(node->left, aid);
        }
        else {
            node->right = Remove(node->right, aid);
        }

        if (!node) return node;

        UpdateHeight(node);
        int balance = BalanceFactor(node);

        if (balance > 1 && BalanceFactor(node->left) >= 0)
            return RotateRight(node);
        if (balance > 1 && BalanceFactor(node->left) < 0) {
            node->left = RotateLeft(node->left);
            return RotateRight(node);
        }
        if (balance < -1 && BalanceFactor(node->right) <= 0)
            return RotateLeft(node);
        if (balance < -1 && BalanceFactor(node->right) > 0) {
            node->right = RotateRight(node->right);
            return RotateLeft(node);
        }

        return node;
    }

    void Destroy(AVLNode* node) {
        if (node) {
            Destroy(node->left);
            Destroy(node->right);
            delete node;
        }
    }

    void FindByTimeRange(AVLNode* node, time_t start, time_t end, vector<shared_ptr<Appointment>>& result) {
        if (!node) return;
        if (node->appointment->time >= start && node->appointment->time <= end && node->appointment->is_valid) {
            result.push_back(node->appointment);
        }
        if (node->appointment->time > start) {
            FindByTimeRange(node->left, start, end, result);
        }
        if (node->appointment->time < end) {
            FindByTimeRange(node->right, start, end, result);
        }
    }

public:
    AVLTree() : root(nullptr) {}

    void Insert(shared_ptr<Appointment> app) {
        root = Insert(root, app);
    }

    void Remove(string aid) {
        root = Remove(root, aid);
    }

    vector<shared_ptr<Appointment>> FindByTimeRange(time_t start, time_t end) {
        vector<shared_ptr<Appointment>> result;
        FindByTimeRange(root, start, end, result);
        return result;
    }

    ~AVLTree() {
        Destroy(root);
    }
};

struct PQNode {
    shared_ptr<Appointment> appointment;
    PQNode(shared_ptr<Appointment> app) : appointment(app) {}
};

struct PriorityQueue {
private:
    PQNode** heap;
    int capacity;
    int size;

    int Parent(int i) { return (i - 1) / 2; }
    int Left(int i) { return 2 * i + 1; }
    int Right(int i) { return 2 * i + 2; }

    void Heapify(int i) {
        int smallest = i;
        int left = Left(i);
        int right = Right(i);
        if (left < size && heap[left]->appointment->time < heap[smallest]->appointment->time)
            smallest = left;
        if (right < size && heap[right]->appointment->time < heap[smallest]->appointment->time)
            smallest = right;
        if (smallest != i) {
            swap(heap[i], heap[smallest]);
            Heapify(smallest);
        }
    }

public:
    PriorityQueue(int cap = 100) : capacity(cap), size(0) {
        heap = new PQNode * [capacity];
    }

    void Push(shared_ptr<Appointment> app) {
        if (size >= capacity) throw runtime_error("Hàng đợi ưu tiên đã đầy");
        heap[size] = new PQNode(app);
        int i = size++;
        while (i > 0 && heap[Parent(i)]->appointment->time > heap[i]->appointment->time) {
            swap(heap[i], heap[Parent(i)]);
            i = Parent(i);
        }
    }

    shared_ptr<Appointment> Pop() {
        if (size == 0) throw runtime_error("Hàng đợi ưu tiên rỗng");
        while (size > 0 && !heap[0]->appointment->is_valid) {
            delete heap[0];
            heap[0] = heap[--size];
            Heapify(0);
        }
        if (size == 0) throw runtime_error("Hàng đợi ưu tiên rỗng");
        shared_ptr<Appointment> result = heap[0]->appointment;
        delete heap[0];
        heap[0] = heap[--size];
        Heapify(0);
        return result;
    }

    bool IsEmpty() {
        while (size > 0 && !heap[0]->appointment->is_valid) {
            delete heap[0];
            heap[0] = heap[--size];
            Heapify(0);
        }
        return size == 0;
    }

    ~PriorityQueue() {
        for (int i = 0; i < size; i++) delete heap[i];
        delete[] heap;
    }
};

struct DLLNode {
    shared_ptr<Appointment> appointment;
    DLLNode* prev;
    DLLNode* next;
    DLLNode(shared_ptr<Appointment> app) : appointment(app), prev(nullptr), next(nullptr) {}
};

struct DoublyLinkedList {
private:
    DLLNode* head;
    DLLNode* tail;

public:
    DoublyLinkedList() : head(nullptr), tail(nullptr) {}

    void Append(shared_ptr<Appointment> app) {
        DLLNode* newNode = new DLLNode(app);
        if (!head) {
            head = tail = newNode;
            return;
        }
        tail->next = newNode;
        newNode->prev = tail;
        tail = newNode;
    }

    void Remove(string appointment_id) {
        DLLNode* current = head;
        while (current) {
            if (current->appointment->appointment_id == appointment_id) {
                if (current->prev) current->prev->next = current->next;
                else head = current->next;
                if (current->next) current->next->prev = current->prev;
                else tail = current->prev;
                delete current;
                return;
            }
            current = current->next;
        }
    }

    vector<shared_ptr<Appointment>> FindByDoctor(string doctor_id) {
        vector<shared_ptr<Appointment>> result;
        DLLNode* current = head;
        while (current) {
            if (current->appointment->doctor_id == doctor_id && current->appointment->is_valid) {
                result.push_back(current->appointment);
            }
            current = current->next;
        }
        return result;
    }

    ~DoublyLinkedList() {
        DLLNode* current = head;
        while (current) {
            DLLNode* temp = current;
            current = current->next;
            delete temp;
        }
    }
};

#endif 