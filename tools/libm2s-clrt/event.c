/*
 *  Multi2Sim
 *  Copyright (C) 2012  Rafael Ubal (ubal@ece.neu.edu)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <unistd.h>
#include <stdio.h>

#include <m2s-clrt.h>

struct clrt_finish_t
{
	char c;
};




/*
 * Private Functions 
 */

void clrt_finish_action(void *data)
{
	/* do nothing. */
}


void clrt_wait(struct _cl_event *event)
{
	if (event->queue)
	{
		pthread_mutex_lock(&event->queue->lock);
		if (event->queue->head != NULL && event->queue->process == 0)
		{
			event->queue->process = 1;
			pthread_cond_signal(&event->queue->cond_process);
		}
		pthread_mutex_unlock(&event->queue->lock);
	}
	pthread_mutex_lock(&event->mutex);
	while (event->status != CL_COMPLETE)
		pthread_cond_wait(&event->cond, &event->mutex);
	pthread_mutex_unlock(&event->mutex);
}


void clrt_event_free(void *data)
{
	struct _cl_event *event;

	event = (struct _cl_event *) data;
	pthread_mutex_destroy(&event->mutex);
	pthread_cond_destroy(&event->cond);
	free(event);
}


void clrt_event_set_status(struct _cl_event *event, int status)
{
	pthread_mutex_lock(&event->mutex);
	event->status = status;
	if (status == CL_COMPLETE)
		pthread_cond_broadcast(&event->cond);
	pthread_mutex_unlock(&event->mutex);
}


struct _cl_event *clrt_event_create(struct _cl_command_queue *queue)
{
	struct _cl_event *event;

	event = (struct _cl_event *) malloc(sizeof (struct _cl_event));
	if (event == NULL)
		fatal("%s: out of memory", __FUNCTION__);
	clrt_object_create(event, CLRT_OBJECT_EVENT, clrt_event_free);
	

	event->status = CL_QUEUED;
	event->queue = queue;
	pthread_mutex_init(&event->mutex, NULL);
	pthread_cond_init(&event->cond, NULL);
	return event;
}


int clrt_event_wait_list_check(
	unsigned int num_events, 
	struct _cl_event * const *event_list)
{
	unsigned int i;

	if ((event_list == NULL && num_events != 0) 
		|| (event_list != NULL && num_events == 0))
		return CL_INVALID_EVENT_WAIT_LIST;

	/* Verify that the parameter list is valid up-front */
	for (i = 0; i < num_events; i++)
	{
		if (!clrt_object_verify(event_list[i], CLRT_OBJECT_EVENT))
			return CL_INVALID_EVENT_WAIT_LIST;
	}
	return CL_SUCCESS;
}




/*
 * Public Functions 
 */

cl_int clWaitForEvents(
	cl_uint num_events,
	const cl_event *event_list)
{
	cl_uint i;
	
	/* Debug */
	m2s_clrt_debug("call '%s'", __FUNCTION__);
	m2s_clrt_debug("\tnum_events = %d", num_events);
	m2s_clrt_debug("\tevent_list = %p", event_list);

	if (num_events == 0 || event_list == NULL)
		return CL_INVALID_VALUE;

	/* Verify that the parameter list is valid up-front */
	for (i = 0; i < num_events; i++)
	{
		if (!clrt_object_verify(event_list[i], CLRT_OBJECT_EVENT))
			return CL_INVALID_EVENT;
	}

	for (i = 0; i < num_events; i++)
		clrt_wait(event_list[i]);

	return CL_SUCCESS;
}


cl_int clGetEventInfo(
	cl_event event,
	cl_event_info param_name,
	size_t param_value_size,
	void *param_value,
	size_t *param_value_size_ret)
{
	__M2S_CLRT_NOT_IMPL__
	return 0;
}


cl_event clCreateUserEvent(
	cl_context context,
	cl_int *errcode_ret)
{
	struct _cl_event *event;
	
	/* Debug */
	m2s_clrt_debug("call '%s'", __FUNCTION__);
	m2s_clrt_debug("\tcontext = %p", context);
	m2s_clrt_debug("\terrcode_ret = %p", errcode_ret);

	event = (struct _cl_event *) malloc(sizeof (struct _cl_event));
	if (event == NULL)
		fatal("%s: out of memory", __FUNCTION__);
	
	/* check to see that context is valid */
	if (!clrt_object_verify(context, CLRT_OBJECT_CONTEXT))
	{
		if (errcode_ret != NULL)
			*errcode_ret = CL_INVALID_CONTEXT;
		return NULL;
	}

	clrt_object_create(event, CLRT_OBJECT_EVENT, clrt_event_free);
	
	event->status = CL_QUEUED;
	event->changed = CL_FALSE;
	pthread_mutex_init(&event->mutex, NULL);
	pthread_cond_init(&event->cond, NULL);
	return event;
}


cl_int clRetainEvent(
	cl_event event)
{
	/* Debug */
	m2s_clrt_debug("call '%s'", __FUNCTION__);
	m2s_clrt_debug("\tevent = %p", event);
	
	return clrt_object_retain(event, CLRT_OBJECT_EVENT, CL_INVALID_EVENT);
}


cl_int clReleaseEvent(
	cl_event event)
{
	/* Debug */
	m2s_clrt_debug("call '%s'", __FUNCTION__);
	m2s_clrt_debug("\tevent = %p", event);

	return clrt_object_release(event, CLRT_OBJECT_EVENT, CL_INVALID_EVENT);
}


cl_int clSetUserEventStatus(
	cl_event event,
	cl_int execution_status)
{
	/* Debug */
	m2s_clrt_debug("call '%s'", __FUNCTION__);
	m2s_clrt_debug("\tevent = %p", event);
	m2s_clrt_debug("\texecution_status = %d", execution_status);

	if (!clrt_object_verify(event, CLRT_OBJECT_EVENT) || event->queue)
		return CL_INVALID_EVENT;

	if (event->status > CL_COMPLETE)
		return CL_INVALID_VALUE;

	if(event->changed == CL_TRUE)
		return CL_INVALID_OPERATION;

	if (execution_status <= CL_COMPLETE)
	{
		clrt_event_set_status(event, execution_status);
		event->changed = CL_TRUE;
		return CL_SUCCESS;
	}

	return CL_INVALID_VALUE;	
}


cl_int clSetEventCallback(
	cl_event event,
	cl_int command_exec_callback_type,
	void (*pfn_notify)(cl_event , cl_int , void *),
	void *user_data)
{
	__M2S_CLRT_NOT_IMPL__
	return 0;
}


cl_int clGetEventProfilingInfo(
	cl_event event,
	cl_profiling_info param_name,
	size_t param_value_size,
	void *param_value,
	size_t *param_value_size_ret)
{
	__M2S_CLRT_NOT_IMPL__
	return 0;
}


cl_int clFlush(
	cl_command_queue command_queue)
{
	/* Debug */
	m2s_clrt_debug("call '%s'", __FUNCTION__);
	m2s_clrt_debug("\tcommand_queue = %p", command_queue);

	if (!clrt_object_verify(command_queue, CLRT_OBJECT_COMMAND_QUEUE))
		return CL_INVALID_COMMAND_QUEUE;

	pthread_mutex_lock(&command_queue->lock);

	if (command_queue->head != NULL && command_queue->process == 0)
	{
		command_queue->process = 1;
		pthread_cond_signal(&command_queue->cond_process);
	}

	pthread_mutex_unlock(&command_queue->lock);
	return CL_SUCCESS;
}


cl_int clFinish(
	cl_command_queue command_queue)
{
	struct clrt_finish_t *finish;
	struct clrt_queue_item_t *item;

	/* Debug */
	m2s_clrt_debug("call '%s'", __FUNCTION__);
	m2s_clrt_debug("\tcommand_queue = %p", command_queue);

	if (!clrt_object_verify(command_queue, CLRT_OBJECT_COMMAND_QUEUE))
		return CL_INVALID_COMMAND_QUEUE;

	cl_event event = clrt_event_create(command_queue);

	finish = (struct clrt_finish_t *) malloc(sizeof (struct clrt_finish_t));
	if (finish == NULL)
		fatal("%s: out of memory", __FUNCTION__);

	item = clrt_queue_item_create(
		command_queue,
		finish,
		clrt_finish_action, 
		&event, 
		0, 
		NULL);

	clrt_command_queue_enqueue(command_queue, item);

	clrt_wait(event);

	clReleaseEvent(event);

	return CL_SUCCESS;	
}

