#include <cassert>
#include <iostream>

#define VERBOSE

#include <algorithm>

#include "executive.h"
#include "rt/priority.h"
#include "rt/affinity.h"

std::mutex Executive::task_mutex;//
std::vector<std::unique_ptr<std::condition_variable>> Executive::cond; //TODO: capire come fare ad inizializzare il vettore di cond

Executive::Executive(size_t num_tasks, unsigned int frame_length, unsigned int unit_duration)
	: p_tasks(num_tasks), frame_length(frame_length), unit_time(unit_duration)
{
}


void Executive::set_periodic_task(size_t task_id, std::function<void()> periodic_task, unsigned int /* wcet */)
{
	assert(task_id < p_tasks.size()); // Fallisce in caso di task_id non corretto (fuori range)
	
	p_tasks[task_id].function = periodic_task;
}
		
void Executive::add_frame(std::vector<size_t> frame) // frame contine gli indici dei task, quindi è un singlolo frame
{
	for (auto & id: frame)
		assert(id < p_tasks.size()); // Fallisce in caso di task_id non corretto (fuori range)
	
	frames.push_back(frame); // frames è tutta la schedule

	/* ... */
}

void Executive::start()
{
	rt::priority p(rt::priority::rt_max);
	rt::affinity a;
	a.set(1);

	for (size_t id = 0; id < p_tasks.size(); ++id)
	{
		assert(p_tasks[id].function); // Fallisce se set_periodic_task() non e' stato invocato per questo id
		
		p_tasks[id].thread = std::thread(&Executive::task_function, std::ref(p_tasks[id]));
		p_tasks[id].state = task_data::IDLE;
		rt::set_affinity(p_tasks[id].thread, a);

		
		/* ... */
	}
	
	exec_thread = std::thread(&Executive::exec_function, this);
	rt::set_priority(exec_thread, p);
	rt::set_affinity(exec_thread, a);
	/* ... */
}
	
void Executive::wait()
{
	exec_thread.join();
	
	for (auto & pt: p_tasks)
		pt.thread.join();
}

void Executive::task_function(Executive::task_data & task)
{
	while (true)
	{
		std::unique_lock<std::mutex> lock(Executive::task_mutex);
		while (task.state != task_data::PENDING)
		{
			Executive::cond[task.id]->wait(lock);
		}
		task.state = task_data::RUNNING;
		task.function();
		task.state = task_data::IDLE;
	}
}

void Executive::exec_function()
{
	int frame_id = 0;
	auto clock = std::chrono::high_resolution_clock::now();
	std::vector<size_t> deadline_miss;

	/* ... */

	while (true)
	{
		#ifdef VERBOSE
		std::cout << "*** Frame n." << frame_id << (frame_id == 0 ? " ******" : "") << std::endl;
		#endif
		/* Rilascio dei task periodici del frame corrente ... */
		rt::priority p(rt::priority::rt_max); //priorità che verrà decrementatata ed assegnata ai task

		for (size_t task_id = 0; task_id < frames[frame_id].size(); ++task_id)
		{
			try{
				size_t num_task = frames[frame_id][task_id];
				// Controllo se il task che sto per schedulare è in coda fra quelli missati
				if (std::find(deadline_miss.begin(), deadline_miss.end(), num_task) != deadline_miss.end()) {
					p_tasks[num_task].state = task_data::PENDING;
					rt::set_priority(p_tasks[num_task].thread, --p);
					Executive::cond[num_task]->notify_one();
				}

			} catch(rt::permission_error &e){
				std::cout << "Failed to set priority: " << e.what() << std::endl;
			}
		}

		for (size_t i = 0; i < deadline_miss.size(); ++i)
		{
			if (p != rt::priority::rt_min)
			{
				--p;
			}
			rt::set_priority(p_tasks[deadline_miss[i]].thread, p);
		}

		/* Attesa fino al prossimo inizio frame ... */
		clock += frame_length * unit_time;
		std::this_thread::sleep_until(clock);

		/* Controllo delle deadline ... */
		for (int i = (int)deadline_miss.size() - 1; i >= 0; --i)
		{
			if (p_tasks[deadline_miss[i]].state != task_data::RUNNING)
			{
				deadline_miss.erase(deadline_miss.begin() + i);
			}
		}


		for (size_t task_id = 0; task_id < frames[frame_id].size(); ++task_id)
		{
			if (p_tasks[task_id].state == task_data::RUNNING)
			{
				deadline_miss.push_back(task_id);

			} else if (p_tasks[task_id].state == task_data::PENDING)
			{
				p_tasks[task_id].state = task_data::IDLE; // se il task non è ancora entrato in esecuzione per colpa di una deadline lo abortisco
			}
		}


		
		if (++frame_id == frames.size())
		{
			frame_id = 0;
		}
	}
}


