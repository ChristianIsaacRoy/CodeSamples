#include <stdlib.h>
#include <stdio.h>
#include "csapp.h"
#include "ast.h"
#include "fail.h"

static void run_script(script *scr);
static void run_group(script_group *group);
static void run_and_group(script_group *group);
static void run_or_group(script_group *group);
static void run_command(script_command *command);
static void set_var(script_var *var, int new_value);
static void ctrlc(int sig);

static pid_t *pids;
int len;

int main(int argc, char **argv) {
  script *scr;
  
  if ((argc != 1) && (argc != 2)) {
    fprintf(stderr, "usage: %s [<script-file>]\n", argv[0]);
    exit(1);
  }

  scr = parse_script_file((argc > 1) ? argv[1] : NULL);

  run_script(scr);

  return 0;
}

static void run_script(script *scr) {
  Signal(SIGINT, ctrlc);
  int i;
  int group_num = scr->num_groups;
  for (i=0; i<group_num; i++){
    run_group(&scr->groups[i]);
  }
}

static void run_group(script_group *group) {

  // Loop for handling repeats
  int i, repeat_num = group->repeats;
  for (i=0; i<repeat_num; i++) {

    int j;
    int commands_num = group->num_commands;
    int child_status;
    len = commands_num;

    if (pids != NULL){
      Free (pids);
    }
    pids = Malloc(group->num_commands*sizeof(pid_t));

    // If there is only a single command
    if (group->mode == GROUP_SINGLE){
      for (j=0; j<commands_num; j++ ){
        pids[j] = Fork();
        if (pids[j] == 0){
          setpgid(0, 0);
          run_command(&group->commands[j]);
        }
        Wait(&child_status);
      }
      if (group->result_to != NULL){
        // If the command was interrupted with a signal
        if (WIFSIGNALED(child_status)){
          set_var(group->result_to, -WTERMSIG(child_status));
        }
        // If the command exited with an error
        if (WIFEXITED(child_status)){
          set_var(group->result_to, WEXITSTATUS(child_status));
        }
      }
    }

    // If the group is piped together
    else if (group->mode == GROUP_AND){
      run_and_group(group);
    }

    // If the group is an OR_GROUP
    else if (group->mode == GROUP_OR) {
      run_or_group(group);
    }

  }// End loop for repeats
}

static void run_or_group(script_group *group){
  int i, status;

  for (i=0; i<group->num_commands; i++){
    pids[i] = Fork();
    if (pids[i] == 0){
      setpgid(0, 0);
      run_command(&group->commands[i]);
    }
    if (group->commands[i].pid_to != NULL){
      set_var(group->commands[i].pid_to, pids[i]);
    }
  }

  pid_t first = Wait(&status);
  if (group->result_to != NULL){
    if (WIFEXITED(status))
      set_var(group->result_to, WEXITSTATUS(status));
    else
      set_var(group->result_to, -WTERMSIG(status));
  }

  for (i=0; i<group->num_commands; i++){
    if (first != pids[i]){
      Kill(pids[i], SIGTERM);
      Waitpid(pids[i], &status, 0);
    }
  }
}

static void run_and_group(script_group *group){

  int j;
  int commands_num = group->num_commands;
  int child_status;

  // An array of pipes
  int fds_arr[commands_num-1][2];
  for (j=0; j<commands_num-1; j++){
    Pipe(fds_arr[j]);
  }

  sigset_t sigs;
  sigemptyset(&sigs);
  sigaddset(&sigs, SIGINT);
  sigprocmask(SIG_BLOCK, &sigs, NULL);

  // The first command
  pids[0] = Fork();
  if (pids[0] == 0){
    setpgid(0, 0);
    Dup2(fds_arr[0][1], 1);
    int k;
    for (k=0; k<commands_num-1; k++){
      Close(fds_arr[k][0]);
      Close(fds_arr[k][1]);
    }
    run_command(&group->commands[0]);
  }
  if (group->commands[0].pid_to != NULL){
    set_var(group->commands[0].pid_to, pids[0]);
  }

  // All of the middle commands
  for (j=1; j<commands_num-1; j++){
    pids[j] = Fork();
    if (pids[j] == 0){
      setpgid(0, 0);
      Dup2(fds_arr[j][1], 1);
      Dup2(fds_arr[j-1][0], 0);
      int k;
      for (k=0; k<commands_num-1; k++){
        Close(fds_arr[k][0]);
        Close(fds_arr[k][1]);
      }
      run_command(&group->commands[j]);
    }
    if (group->commands[j].pid_to != NULL){
      set_var(group->commands[j].pid_to, pids[j]);
    }
  }

  for (j=0; j<commands_num-2; j++){
    Close(fds_arr[j][0]);
    Close(fds_arr[j][1]);
  }

  // The last command
  pids[commands_num-1] = Fork();
  if (pids[commands_num-1] == 0){
    setpgid(0,0);
    Dup2(fds_arr[commands_num-2][0], 0);
    Close(fds_arr[commands_num-2][0]);
    Close(fds_arr[commands_num-2][1]);
    run_command(&group->commands[commands_num-1]);
  }
  if (group->commands[commands_num-1].pid_to != NULL){
    set_var(group->commands[commands_num-1].pid_to, pids[commands_num-1]);
  }
  Close(fds_arr[commands_num-2][0]);
  Close(fds_arr[commands_num-2][1]);

  sigprocmask(SIG_UNBLOCK, &sigs, NULL);

  // Reap all the processes
  for (j=0; j<commands_num; j++){
    Waitpid(pids[j], &child_status, 0);
  }

  if (group->result_to != NULL){
    if (WIFEXITED(child_status))
      set_var(group->result_to, WEXITSTATUS(child_status));
    else
      set_var(group->result_to, -WTERMSIG(child_status));
  }
}

static void run_command(script_command *command) {
  const char **argv;
  int i;

  argv = malloc(sizeof(char *) * (command->num_arguments + 2));
  argv[0] = command->program;

  for (i = 0; i < command->num_arguments; i++) {
    if (command->arguments[i].kind == ARGUMENT_LITERAL)
      argv[i+1] = command->arguments[i].u.literal;
    else{
      argv[i+1] = command->arguments[i].u.var->value;
    }
  }

  argv[command->num_arguments + 1] = NULL;

  Execve(argv[0], (char * const *)argv, environ);

  free(argv);
}

static void ctrlc(int sig){
  int i;
  for (i=0; i<len; i++){
    Kill(pids[i], SIGTERM);
  }
}

static void set_var(script_var *var, int new_value) {
  char buffer[32];
  free((void *)var->value);
  snprintf(buffer, sizeof(buffer), "%d", new_value);
  var->value = strdup(buffer);
}