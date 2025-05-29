// 35. Дана прямоугольная целочисленная матрица. Распараллеливание по строкам.
// Найти количество строк, состоящих только из четных элементов.
// 4 задача: атомарные типы
// 5 задача: Queue, Mutex C++

#include <iostream>
#include <fstream>
#include <process.h>
#include <Windows.h>
#include <thread>
#include <future>
#include <atomic>
#include "ThreadsafeQueue.h"
#include <random>
#include <omp.h>

constexpr size_t COLS = 5; // Количество столбцов (размер блока)
constexpr size_t ROWS = 5; // Количество строк
constexpr size_t MATR_SIZE = COLS * ROWS; // Размер матрицы
constexpr size_t NTHREADS = ROWS; //Кол-во потоков (распараллеливание по строкам)

struct INFORM
{
	int** matrix;
	size_t row_index;
	bool all_even;
};


// matrix interface
int** memory_allocation()
{
	int** matrix = new int* [ROWS];
	for (int i{}; i < ROWS; ++i)
		matrix[i] = new int[COLS];
	return matrix;
}

void free_memory(int**& matrix)
{
	for (int i{}; i < ROWS; ++i)
		delete[]matrix[i];
	delete[]matrix;
}

void init_matrix(int** matrix, int left, int right)
{
	for (size_t i{}; i < ROWS; ++i)
		for (size_t j{}; j < COLS; ++j)
			matrix[i][j] = left + rand() % (right - left);
}

void init_matrix(int** matrix, std::ifstream& file)
{
	for (size_t i{}; i < ROWS; ++i)
		for (size_t j{}; j < COLS; ++j)
			file >> matrix[i][j];
}

void print_matrix(int** matrix)
{
	for (size_t i{}; i < ROWS; ++i)
	{
		for (size_t j{}; j < COLS; ++j)
			std::cout << matrix[i][j] << ' ';
		std::cout << '\n';
	}
}


// common task
bool is_all_even(int* arr)
{
	bool result{ true };
	for (size_t i{}; i < COLS && result; ++i)
		if (arr[i] % 2 != 0)
			result = false;
	return result;
}

int all_even_rows_count(int** matrix)
{
	int count{};
	for (size_t i{}; i < ROWS; ++i)
	{
		int* row = matrix[i];
		if (is_all_even(row))
			count++;
	}
	return count;
}


// task 1
unsigned __stdcall is_all_even_parallel(void* arg) // in the one row
{
	INFORM* inform = (INFORM*)arg;
	int* row = inform->matrix[inform->row_index];
	inform->all_even = is_all_even(row);
	_endthreadex(0);
	return 0;
}

int all_even_rows_count_parallel(int** matrix)
{
	HANDLE t[NTHREADS];
	INFORM inform[NTHREADS];
	for (size_t i{}; i < NTHREADS; ++i)
	{
		inform[i].matrix = matrix;
		inform[i].row_index = i;
		t[i] = (HANDLE)_beginthreadex(nullptr, 0, &is_all_even_parallel, inform + i, 0, nullptr);
	}

	WaitForMultipleObjects(NTHREADS, t, true, INFINITE);

	int count{};
	for (size_t i{}; i < NTHREADS; ++i)
	{
		if (inform[i].all_even)
			count++;
		CloseHandle(t[i]);
	}
	return count;
}

// task 2
void is_all_even_ref(int* arr, bool& result)
{
	result = true;
	for (size_t i{}; i < COLS && result; ++i)
		if (arr[i] % 2 != 0)
			result = false;
}

int all_even_rows_count_parallel2(int** matrix)
{
	std::thread t[NTHREADS];
	bool results[NTHREADS];

	for (size_t i{}; i < NTHREADS; ++i)
	{
		int* row = matrix[i];
		t[i] = std::thread(is_all_even_ref, row, std::ref(results[i]));
	}

	for (size_t i = 0; i < NTHREADS; ++i)
		t[i].join();

	int count{};
	for (size_t i = 0; i < NTHREADS; ++i)
		if (results[i])
			count++;

	return count;
}

// task 3
int all_even_rows_count_parallel3(int** matrix)
{
	std::future<bool> t[NTHREADS];
	for (size_t i{}; i < NTHREADS; ++i)
	{
		int* row = matrix[i];
		t[i] = std::async(std::launch::async, is_all_even, row);
	}

	int count{};
	for (size_t i{}; i < NTHREADS; ++i)
		if (t[i].get())
			count++;
	return count;
}

// task 4 (atomic)
void is_all_even_atomic(int* arr, std::atomic_bool& result)
{
	result = true;
	for (size_t i{}; i < COLS && result; ++i)
		if (arr[i] % 2 != 0)
			result = false;
}

int all_even_rows_count_parallel4(int** matrix)
{
	std::thread t[NTHREADS];
	std::atomic_bool results[NTHREADS];

	for (size_t i{}; i < NTHREADS; ++i)
	{
		int* row = matrix[i];
		t[i] = std::thread(is_all_even_atomic, row, std::ref(results[i]));
	}

	for (size_t i = 0; i < NTHREADS; ++i)
		t[i].join();

	int count{};
	for (size_t i = 0; i < NTHREADS; ++i)
		if (results[i])
			count++;
	return count;
}

// task 5 (Queue, Mutex C++)
ThreadsafeQueue<int> que_int;

void is_all_even_queue(int* arr, std::atomic_bool& result)
{
	int row{};
	if (que_int.try_pop(row)) // извлекаем индекс строки из очереди
	{
		result = true;
		for (size_t i{}; i < COLS && result; ++i)
			if (arr[i] % 2 != 0)
				result = false;
	}
	else
		result = false;
}

int all_even_rows_count_parallel5(int** matrix)
{
	std::thread t[NTHREADS];
	std::atomic_bool results[NTHREADS];

	// Заполняем очередь индексами строк
	for (size_t i{}; i < NTHREADS; ++i)
		que_int.push(i);

	for (size_t i{}; i < NTHREADS; ++i)
	{
		int* row = matrix[i];
		t[i] = std::thread(is_all_even_queue, row, std::ref(results[i]));
	}

	for (size_t i = 0; i < NTHREADS; ++i)
		t[i].join();

	int count{};
	for (size_t i = 0; i < NTHREADS; ++i)
		if (results[i])
			count++;
	return count;
}

// task 6
volatile long volume_work_producer = ROWS;
volatile long volume_work_consumer = ROWS;
std::mutex mut;
std::condition_variable cv;
ThreadsafeQueue<int*> que_ptr_int;

void task_producer()
{
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dist(0, 50); // 0..50, чтобы после *2 не выйти за 100

    while (_InterlockedExchangeAdd(&volume_work_producer, -1) > 0)
    {
    int* row = new int[COLS];
    for (size_t i{}; i < COLS; ++i)
    {
    row[i] = dist(gen) * 2; // генерируем только чётные числа
    std::cout << row[i] << ' ';
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    std::cout << '\n';
    que_ptr_int.push(row);
    cv.notify_all();
    }

	while (_InterlockedExchangeAdd(&volume_work_producer, -1) > 0)
	{
		int* row = new int[COLS];
		for (size_t i{}; i < COLS; ++i)
		{
			row[i] = dist(gen);
			std::cout << row[i] << ' ';
			std::this_thread::sleep_for(std::chrono::milliseconds(200));
		}
		std::cout << '\n';
		que_ptr_int.push(row);
		cv.notify_all();
	}
}

void task_consumer(int& count)
{
	count = 0;
	bool all_even{};

	while (_InterlockedExchangeAdd(&volume_work_consumer, -1) > 0)
	{
		int* row = nullptr;
		std::unique_lock<std::mutex> locker(mut);
		cv.wait_for(locker, std::chrono::seconds(5), []() {return !que_ptr_int.empty(); });
		if (que_ptr_int.try_pop(row))
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(400));
			all_even = is_all_even(row);
			std::cout << std::boolalpha << all_even << '\n';
			if (all_even)
				count++;
			delete[] row;
		}
		else
		{
			_InterlockedExchangeAdd(&volume_work_consumer, 1);
		}
	}
}

// task 7 (OpenMP)
int all_even_rows_count_parallel7(int** matrix)
{
	int count{};
	#pragma omp parallel for reduction(+:count)
	for (size_t i{}; i < ROWS; ++i)
	{
		if (is_all_even(matrix[i]))
			count++;
	}
	return count;
}



int main()
{
	srand(time(NULL));

	int** matrix = memory_allocation();
	std::ifstream file("data.txt");
	init_matrix(matrix, file);

	std::cout << "Matrix:\n";
	print_matrix(matrix);
	std::cout << "\nNon parallel solution\nCount of all-even rows = " << all_even_rows_count(matrix) << '\n';

	std::cout << "\nTask 1:\n";
	std::cout << "Parallel solution = " << all_even_rows_count_parallel(matrix) << '\n';

	std::cout << "\nTask 2:\n";
	std::cout << "Parallel solution = " << all_even_rows_count_parallel2(matrix) << '\n';

	std::cout << "\nTask 3:\n";
	std::cout << "Parallel solution = " << all_even_rows_count_parallel3(matrix) << '\n';

	std::cout << "\nTask 4:\n";
	std::cout << "Parallel solution = " << all_even_rows_count_parallel4(matrix) << '\n';

	std::cout << "\nTask 5:\n";
	std::cout << "Parallel solution = " << all_even_rows_count_parallel5(matrix) << '\n';

	std::cout << "\nTask 6:\n";

	int count{};
	std::thread workers[NTHREADS];
	for (int i{}; i < NTHREADS; ++i)
	{
		if (i < 2)
			workers[i] = std::thread(task_producer);
		else
			workers[i] = std::thread(task_consumer, std::ref(count));
	}

	for (int i = 0; i < NTHREADS; ++i)
		workers[i].join();
	std::cout << "Parallel solution = " << count << '\n';

	std::cout << "\nTask 7:\n";
	std::cout << "Parallel solution = " << all_even_rows_count_parallel7(matrix) << '\n';

	free_memory(matrix);

	return 0;
}