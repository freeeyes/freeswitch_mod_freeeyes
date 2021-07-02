#include <switch.h>
#include <unistd.h>
#include "thpool.h"

//处理会议踢人播放一段语音后的问题。
//add by freeeyes

/*
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_example_shutdown);
SWITCH_MODULE_RUNTIME_FUNCTION(mod_example_runtime);
*/

SWITCH_MODULE_LOAD_FUNCTION(mod_freeeyes_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_freeeyes_shutdown);
SWITCH_MODULE_DEFINITION(mod_freeeyes, mod_freeeyes_load, NULL, NULL);

// cmd为参数列表
// sessin为当前callleg的session
// stream为当前输出流。如果想在Freeswitch控制台中输出什么，可以往这个流里写
#define SWITCH_STANDARD_API(name)  \
static switch_status_t name (_In_opt_z_ const char *cmd, _In_opt_ switch_core_session_t *session, _In_ switch_stream_handle_t *stream)

//线程消息处理队列
threadpool the_thread_pool_;

struct task_arg{
	int  cmd_type;
	char* cmd[300];
	switch_core_session_t *session;
	switch_stream_handle_t *stream;
};

//判断一个命令行里有多少空格
int get_cmd_string_space_count(const char* cmd)
{
	int arg_count = 1;
	int cmd_size = strlen(cmd);
	int i = 0;
	
	for(i = 0; i < cmd_size; i++)
	{
		if(cmd[i] == ' ')
		{
			arg_count++;
		}
	}

	return arg_count;
}

//调用命令行执行函数
SWITCH_DECLARE(int) executeString(const char *cmd, switch_core_session_t *session)
{
	char *arg;
	char *mycmd = NULL;
	switch_stream_handle_t stream = { 0 };

	if(cmd == NULL || strlen(cmd) == 0)
	{
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "[executeString]cmd is error.\n");
		return 1;
	}

	mycmd = strdup(cmd);

	if ((arg = strchr(mycmd, ' '))) {
		*arg++ = '\0';
	}

	
	SWITCH_STANDARD_STREAM(stream);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "[executeString]run begin(%s).\n", mycmd);
	switch_api_execute(mycmd, arg, session, &stream);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "[executeString]run end.\n");
	switch_safe_free(mycmd);
	return 0;
}

//全体成员广播并退出
SWITCH_DECLARE(void) do_kick_sound_and_quit(const char *cmd, switch_core_session_t *session)
{
	char *mycmd = NULL;
	char *argv[5] = {0};
	char conferece_cmd[1024] = {'\0'};
	int argc = 0;
	int sleep_time = 0;
	int i = 0;

	int member_id_list_count = 0;
	char member_id_list[100][50];
	char* member_id_user = NULL;
	char* member_id_buffer = NULL;


	memset(member_id_list, 0, 5000);

    mycmd = strdup(cmd); 

	//判断参数个数是否越界
	argc =get_cmd_string_space_count(mycmd);
    if (argc != 5) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "[do_kick_sound_and_quit]parameter number is invalid, mycmd=%s, count=%d.\n", mycmd, argc);
		switch_safe_free(mycmd);
        return;
    }

    argc = switch_split(mycmd, ' ', argv);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "[do_kick_sound_and_quit]confernece name=%s, wav=%s, time wait=%s, memverid=%s.\n", argv[0], argv[1], argv[2], argv[3]);

	//判断memberid字符串是否为空
	if(strlen(argv[3]) == 0)
	{
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "[do_kick_sound_and_quit]confernece name=%s, memberid is null.\n", argv[0]);
		switch_safe_free(mycmd);
		return;
	}

	if(strcmp(argv[3], "all") == 0)
	{
		sprintf(conferece_cmd, "conference %s play %s", argv[0], argv[1]);

		//如果是全员通知
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE,"[do_kick_sound_and_quit]conferece_cmd=%s.\n", conferece_cmd);

		//执行命令(播放语音指令)
		//executeString(conferece_cmd, session);		
	}
	else
	{
		//拆分字符串
		member_id_buffer = (char* )argv[3];
		member_id_user = strsep(&member_id_buffer, ",");
		while(member_id_user!=NULL)
		{
			if(member_id_list_count >= 100)
			{
				//最多100个
				break;
			}

			//判断字符串长度超长
			if(strlen(member_id_user) >= 50)
			{
				break;
			}

			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE,"[do_kick_sound_and_quit]member_id_user=%s.\n", member_id_user);
			if(strcmp(member_id_user, "NULL") == 0)
			{
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE,"[do_kick_sound_and_quit]member_id_user is NULL.\n");
				break;
			}
			
			sprintf(member_id_list[member_id_list_count], "%s", member_id_user);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE,"[do_kick_sound_and_quit]member_id_list(%d)=%s.\n", member_id_list_count, member_id_list[member_id_list_count]);
			member_id_list_count++;

			sprintf(conferece_cmd, "conference %s play %s %s", argv[0], argv[1], member_id_user);

			//一个个发语音通知
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE,"[do_kick_sound_and_quit]conferece_cmd=%s.\n", conferece_cmd);	

			//执行命令(播放语音指令)
			executeString(conferece_cmd, session);						

			member_id_user = strsep(&member_id_buffer, ",");
		}		
	}

	sleep_time = atoi(argv[2]) * 1000;
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE,"[do_kick_sound_and_quit]sleep time=%d.\n", sleep_time);

	if(member_id_list_count > 0)
	{
		//等待语音播放时间
		usleep(sleep_time);
	}

	//指定退出会议命令
	if(strcmp(argv[3], "all") == 0)
	{
		sprintf(conferece_cmd, "conference %s hup all", argv[0]);

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE,"[do_kick_sound_and_quit]conferece_cmd=%s.\n", conferece_cmd);
		//执行命令(播放语音指令)
		executeString(conferece_cmd, session);		
	}
	else
	{
		//一个个的踢出
		for(i = 0; i < member_id_list_count; i++)
		{
			sprintf(conferece_cmd, "conference %s kick %s", argv[0], member_id_list[i]);

			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE,"[do_kick_sound_and_quit]conferece_cmd=%s.\n", conferece_cmd);
			//执行命令(踢人指令)
			executeString(conferece_cmd, session);			
		}

		//查看是否要结束会议
		if(strcmp(argv[4], "over") == 0)
		{
			sprintf(conferece_cmd, "conference %s hup all", argv[0]);

			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE,"[do_kick_sound_and_quit]conferece_cmd=%s.\n", conferece_cmd);
			//执行命令(结束会议)
			executeString(conferece_cmd, session);				
		}
	}

	switch_safe_free(mycmd);
}

//处理任务工作(发送语音，并挂断)
void task(void *arg){
	struct task_arg* task_arg_info = (struct task_arg* )arg;

	if(NULL != task_arg_info)
	{
		//指定相关指令
		if(task_arg_info->cmd_type == 1)    //执行kick全部会议退出指令
		{
			do_kick_sound_and_quit((char* )task_arg_info->cmd, task_arg_info->session);
		}

		//用完释放对象
		free(task_arg_info);
	}
}

//添加处理API的函数
SWITCH_STANDARD_API(conference_kick_sound_function) {
	struct task_arg *task_arg_info = NULL;
    if (zstr(cmd)) {
        stream->write_function(stream, "[conference_kick_sound_function]parameter missing.\n");
        return SWITCH_STATUS_SUCCESS;
    }
    
	//讲数据加入处理线程队列
	task_arg_info = (struct task_arg*)malloc(sizeof(struct task_arg));

	task_arg_info->cmd_type = 1;
	sprintf((char *)task_arg_info->cmd, "%s", cmd);
	task_arg_info->session = session;
	task_arg_info->stream = stream;

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "[conference_kick_sound_function]task_arg_info->cmd=%s!\n", (char* )task_arg_info->cmd);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "[conference_kick_sound_function]push queue!\n");

	thpool_add_work(the_thread_pool_, task, (void*)task_arg_info);

    return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_LOAD_FUNCTION(mod_freeeyes_load)
{
	switch_api_interface_t* commands_api_interface;

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	SWITCH_ADD_API(commands_api_interface, "conference_sound_kick", "play sound and kick user", conference_kick_sound_function, "<conference name> <wav file name> <time wait> <member id>");

	//启动工作线程
	the_thread_pool_ = thpool_init(1);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "load mod_freeeyes!\n");

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

/* Called when the system shuts down */
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_freeeyes_shutdown)
{
	//等待队列中的所有任务执行完毕
	thpool_wait(the_thread_pool_);

	//关闭工作线程
	thpool_destroy(the_thread_pool_);

	return SWITCH_STATUS_SUCCESS;
}

/*
  If it exists, this is called in it's own thread when the module-load completes
  If it returns anything but SWITCH_STATUS_TERM it will be called again automaticly
SWITCH_MODULE_RUNTIME_FUNCTION(mod_example_runtime);
{
	while(looping)
	{
		switch_yield(1000);
	}
	return SWITCH_STATUS_TERM;
}
*/

