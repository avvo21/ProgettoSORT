#include <cassert>
#include <iostream>


#define VERBOSE

#include <algorithm>

#include "executive.h"
#include "rt/priority.h"
#include "rt/affinity.h"


Executive::Executive(size_t num_tasks, unsigned int frame_length, unsigned int unit_duration)
	: p_tasks(num_tasks), frame_length(frame_length), unit_time(unit_duration)
{

}


void Executive::set_periodic_task(size_t task_id, std::function<void()> periodic_task, unsigned int  wcet )
{
	assert(task_id < p_tasks.size()); // Fallisce in caso di task_id non corretto (fuori range)
	
	p_tasks[task_id].function = periodic_task;
	p_tasks[task_id].wcet = wcet;

}

void Executive::set_aperiodic_task(std::function<void()> aperiodic_task, unsigned int wcet)
{
	ap_task.function = aperiodic_task;
	ap_task.wcet = wcet;
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
		p_tasks[id].id = id;
		rt::set_affinity(p_tasks[id].thread, a);
		/* ... */
	}
	
	assert(ap_task.function); // Fallisce se set_aperiodic_task() non e' stato invocato

	ap_task.thread = std::thread(&Executive::task_function, std::ref(ap_task));
	ap_task.requested = false; // Inizializzo la richiesta a false
	rt::set_priority(ap_task.thread, rt::priority::rt_min);
	rt::set_affinity(ap_task.thread, a);

	for (size_t k = 0; k < frames.size(); ++k)
	{
		auto frame = frames[k];
		slack.push_back(frame_length);
		//slack[k] = frame_length;

		for (size_t i = 0; i < frame.size(); ++i)
		{
				slack[k] -= p_tasks[frame[i]].wcet;
		}
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

void Executive::ap_task_request()
{
	std::unique_lock<std::mutex> lock(ap_task.task_mutex);
	ap_task.requested = true;

	/* ...
	 * Si deve fare in modo che l'exec sappia che è stata fatta una request, bool, per poi attivare la politica dell'aperiodico
	 * alla fine dell'esecuzione si riporta a false
	 *
	 * Usare un bool per una richiesta di aperiodico e fare in modo di capire se un task aperiodico è già in esecuzione senza bool
	 *
	 * Qui dobbiamo semplicemente fare un lock e settare la variabile della richiesta a true
	 *
	 * Creare un lock per la manipolazione del bool della richiesta aperiodica
	 *
	 * Controllare la specifica di errore nel caso ci sia una richiesta troppo frequente
	 *
	 * std::unique_lock<std::mutex> lock(ap_task.task_mutex);
	 * if (ap_task.state == task_data::)
	 *
	 *
	 */
}

void Executive::task_function(Executive::task_data & task)
{
	while (true)
	{
		{	//Regione Critica
			std::unique_lock<std::mutex> lock(task.task_mutex);
			while (task.state != task_data::PENDING)
			{
				task.task_cond.wait(lock);
			}
			task.state = task_data::RUNNING;
		}
		task.function();
		{	//Regione Critica
			std::unique_lock<std::mutex> lock(task.task_mutex);
			task.state = task_data::IDLE;
		}
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

		rt::priority p(rt::priority::rt_max); //priorità che verrà decrementatata ed assegnata ai task

		/* Rilascio dei task aperiodici del frame corrente ... */
		if (slack[frame_id] > 0)
		{
			{
				// Regione Critica
				std::unique_lock<std::mutex> lock(ap_task.task_mutex);
				if (ap_task.state == task_data::IDLE && ap_task.requested)
				{
					ap_task.state = task_data::PENDING;
					ap_task.requested = false;
					ap_task.task_cond.notify_one();
					rt::set_priority(ap_task.thread, p-1);
					std::this_thread::sleep_until(clock + (slack[frame_id] * unit_time));
				}
				else if (ap_task.state == task_data::RUNNING)
				{
					rt::set_priority(ap_task.thread, p-1);
					std::this_thread::sleep_until(clock + (slack[frame_id] * unit_time));
				}
			}
			rt::set_priority(ap_task.thread, rt::priority::rt_min);
		}

		/* Rilascio dei task periodici del frame corrente ... */
		for (size_t task_id = 0; task_id < frames[frame_id].size(); ++task_id)
		{
			try{
				size_t num_task = frames[frame_id][task_id];
				// Controllo se il task che sto per schedulare è in coda fra quelli missati
				if (std::find(deadline_miss.begin(), deadline_miss.end(), num_task) == deadline_miss.end()) {
					{	//Regione Critica
						std::unique_lock<std::mutex> lock(p_tasks[num_task].task_mutex);
						p_tasks[num_task].state = task_data::PENDING;
					}
					rt::set_priority(p_tasks[num_task].thread, --p);
					p_tasks[num_task].task_cond.notify_one();
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
		clock += frame_length * unit_time;	//clock += frame_length * unit_time-wCetframe * unit_time;
		std::this_thread::sleep_until(clock);	//aggiornare con


		/* Controllo delle deadline ... */
		for (int i = (int)deadline_miss.size() - 1; i >= 0; --i)
		{
			size_t task_id = deadline_miss[i];
			{	//Regione Criticsa
				std::unique_lock<std::mutex> lock(p_tasks[task_id].task_mutex);
				if (p_tasks[task_id].state != task_data::RUNNING)
				{
					deadline_miss.erase(deadline_miss.begin() + i);
				}
			}
		}

		for (size_t n = 0; n< frames[frame_id].size(); ++n)
		{
			size_t task_id = frames[frame_id][n];
			{	//Regione Critica;
				std::unique_lock<std::mutex> lock(p_tasks[task_id].task_mutex);
				if (p_tasks[task_id].state == task_data::RUNNING)
				{
					std::cerr << "\033[31mTask n." << task_id << " ha missato la deadline!\033[0m" << std::endl;

					deadline_miss.push_back(task_id);

				} else if (p_tasks[task_id].state == task_data::PENDING)
				{
					std::cerr << "\033[31mTask n." << task_id << " ha missato la deadline!\033[0m" << std::endl;
					p_tasks[task_id].state = task_data::IDLE; // se il task non è ancora entrato in esecuzione per colpa di una deadline lo abortisco
				}
			}
		}

		//Controllo Deadline Aperiodici
		{	//Regione Critica
			std::unique_lock<std::mutex> lock(ap_task.task_mutex);
			if (ap_task.state == task_data::RUNNING && ap_task.requested)
			{
				std::cerr << "\033[31mIl Task Aperiodico ha missato la deadline!\033[0m" << std::endl;
				ap_task.requested = false; //Questa politica abortisce il nuovo task aperiodico se il precedente ha missato la deadline
			}
		}

		
		if (++frame_id == frames.size())
		{
			frame_id = 0;
		}
	}
}


