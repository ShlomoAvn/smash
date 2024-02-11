#include <unistd.h>
#include <string.h>
#include <iostream>
#include <vector>
#include <sstream>
#include <sys/wait.h>
#include <iomanip>
#include "Commands.h"

using namespace std;

const std::string WHITESPACE = " \n\r\t\f\v";

#if 0
#define FUNC_ENTRY()  \
  cout << __PRETTY_FUNCTION__ << " --> " << endl;

#define FUNC_EXIT()  \
  cout << __PRETTY_FUNCTION__ << " <-- " << endl;
#else
#define FUNC_ENTRY()
#define FUNC_EXIT()
#define SYS_FAIL -1
#endif

string _ltrim(const std::string& s)
{
  size_t start = s.find_first_not_of(WHITESPACE);
  return (start == std::string::npos) ? "" : s.substr(start);
}

string _rtrim(const std::string& s)
{
  size_t end = s.find_last_not_of(WHITESPACE);
  return (end == std::string::npos) ? "" : s.substr(0, end + 1);
}

string _trim(const std::string& s)
{
  return _rtrim(_ltrim(s));
}

int _parseCommandLine(const char* cmd_line, char** args) {
  FUNC_ENTRY()
  int i = 0;
  std::istringstream iss(_trim(string(cmd_line)).c_str());
  for(std::string s; iss >> s; ) {
    args[i] = (char*)malloc(s.length()+1);
    memset(args[i], 0, s.length()+1);
    strcpy(args[i], s.c_str());
    args[++i] = NULL;
    //CHECK IF MALLOC FAILED IF SO PERROR
  }
  return i;

  FUNC_EXIT()
}

bool _isBackgroundComamnd(const char* cmd_line) {
  const string str(cmd_line);
  return str[str.find_last_not_of(WHITESPACE)] == '&';
}

void _removeBackgroundSign(char* cmd_line) {
  const string str(cmd_line);
  // find last character other than spaces
  unsigned int idx = str.find_last_not_of(WHITESPACE);
  // if all characters are spaces then return
  if (idx == string::npos) {
    return;
  }
  // if the command line does not end with & then return
  if (cmd_line[idx] != '&') {
    return;
  }
  // replace the & (background sign) with space and then remove all tailing spaces.
  cmd_line[idx] = ' ';
  // truncate the command line string up to the last non-space character
  cmd_line[str.find_last_not_of(WHITESPACE, idx) + 1] = 0;
}

//-------------------------------------Helper Functions-------------------------------------



char** getArgs(const char* cmd_line, int* numArgs) {
  char** args = (char**)malloc(COMMAND_MAX_ARGS * sizeof(char**) + 1);
  //initialize to nullptr?
  if (!args) {
   perror("smash error: malloc failed"); 
  }
  *numArgs = _parseCommandLine(cmd_line, args);
  return args;
}

void firstUpdateCurrDir() {
  SmallShell& smash = SmallShell::getInstance();
    char* buffer = (char*)malloc(MAX_PATH_LEGNTH * sizeof(char)+1);
    if (!buffer) {
      free(buffer);
      perror("smash error: malloc failed"); 
    }
    buffer = getcwd(buffer, MAX_PATH_LEGNTH);
    if (!buffer) {
      free(buffer);
      perror("smash error: getcwd failed"); 
    }
    smash.setCurrDir(buffer);
}

bool checkFullPath(char* currPath, char* newPath) {
  int i = 0;
  int minLen = min(string(currPath).length(), string(newPath).length());
  for (; i < minLen; i++) {
    if (currPath[i] != newPath[i]) {
      break;
    }
  }
  if (i > 1) {
    return true;
  }
  return false;
}

char* goUp(char* dir) {
  if (!strcmp(dir, "/")) {
    return dir;
  }
  int cut = string(dir).find_last_of("/");
  dir[cut] = '\0';
  return dir;
}

bool is_number(const std::string &s) {
    std::string::const_iterator it = s.begin();
    //if(*it == '-') ++it; //negative number
    while (it != s.end() && (std::isdigit(*it) || *it == '-')) ++it;
    return !s.empty() && it == s.end();
}
//-------------------------------------SmallShell-------------------------------------

pid_t SmallShell::m_pid = getppid();

SmallShell::SmallShell(std::string prompt) : m_prompt(prompt), m_prevDir(nullptr), m_currDirectory(nullptr) {
   cout << "Constructor called..........\n";
     }

SmallShell::~SmallShell() {
  free(m_prevDir);
  free(m_currDirectory);
}

/**
* Creates and returns a pointer to Command class which matches the given command line (cmd_line)
*/
Command * SmallShell::CreateCommand(const char* cmd_line) {
	// For example:
  string cmd_s = _trim(string(cmd_line));
  string firstWord = cmd_s.substr(0, cmd_s.find_first_of(" \n"));

  if (firstWord.compare("pwd") == 0) {
    return new GetCurrDirCommand(cmd_line);
  }
  else if (firstWord.compare("showpid") == 0) {
    return new ShowPidCommand(cmd_line);
  }
  else if (firstWord.compare("chprompt") == 0) {
    return new ChangePromptCommand(cmd_line);
  }
  else if (firstWord.compare("cd") == 0) {
    return new ChangeDirCommand(cmd_line, &m_prevDir);
  }
  else if (firstWord.compare("jobs") == 0) {
    return new JobsCommand(cmd_line);
  }
  else if (firstWord.compare("quit") == 0) {
   return new QuitCommand(cmd_line, &jobs);
  }
  else if (firstWord.compare("fg") == 0) {
   return new ForegroundCommand(cmd_line, &jobs);
  }
   else if (firstWord.compare("kill") == 0) {
   return new KillCommand(cmd_line, &jobs);
  }
//others
  else {

    //Can a complex command be run in the background?
    bool isBackground = _isBackgroundComamnd(cmd_line);
    int stat = 0;
    char* fixed_cmd = (char*)malloc(MAX_PATH_LEGNTH*sizeof(char)+1);
    strcpy(fixed_cmd, cmd_line);
    _removeBackgroundSign(fixed_cmd);
    ExternalCommand *cmd = new ExternalCommand(fixed_cmd);
    pid_t pid = fork();
    if (pid < 0) {
      perror("smash error: fork failed");
    }
    if (pid > 0 && !isBackground) {
      while ((pid = wait(&stat)) > 0);//if a background son will finish? maybe waitpid
      return nullptr;
    }
    if (pid == 0 && !isBackground) {
      setpgrp();
      return cmd;
    }
    else if  (pid > 0 && isBackground){
      SmallShell &shell = SmallShell::getInstance();
      shell.getJobs()->addJob(cmd, pid);
    }
    else if (pid == 0 && isBackground) {
      setpgrp();
      return cmd;
    }
  }
  return nullptr;
}
JobsList* SmallShell::getJobs(){
  return &jobs;
}

void SmallShell::executeCommand(const char *cmd_line) {
  // TODO: Add your implementation here
  // for example:
  Command* cmd = CreateCommand(cmd_line);
  if (!cmd) {
    return;
  }
  cmd->execute();
  if (dynamic_cast<ExternalCommand*>(cmd) != nullptr) {
    exit(0);
  }
  // Please note that you must fork smash process for some commands (e.g., external commands....)
}

std::string SmallShell::getPrompt() const {
  return m_prompt;
}

void SmallShell::chngPrompt(const std::string newPrompt) {
  m_prompt = newPrompt;
}

char* SmallShell::getCurrDir() const {
  return m_currDirectory;
}
void SmallShell::setCurrDir(char* currDir, char* toCombine) {
  if (toCombine == nullptr) {
    m_currDirectory = currDir;
    return;
  }
  int length = string(currDir).length() + string(toCombine).length() + 1;
  char* temp = (char*)malloc(length * sizeof(char));
  strcpy(temp, currDir);
  strcat(temp, "/");
  strcat(temp, toCombine);
  m_currDirectory = temp;
}

char* SmallShell::getPrevDir() const {
  return m_prevDir;
}
void SmallShell::setPrevDir(char* prevDir){
  m_prevDir = prevDir;
}



//-------------------------------------Jobs-------------------------------------
JobsList::JobEntry::JobEntry(int id, pid_t pid, const char* cmd, bool isStopped): m_id(id), m_pid(pid), m_cmd(cmd), m_isStopped(isStopped){}
 
 
 void JobsList::addJob(Command* cmd, pid_t pid, bool isStopped){
    JobEntry newJob(max_id +1, pid, cmd->gedCmdLine(),isStopped);
    this->m_list.push_back(newJob);
    max_id++;
 }
void JobsList::printJobsList(){
  removeFinishedJobs();
  int i=1;
  for (JobEntry job : m_list) {
     // element.job.second
        std::cout << "["<< i << "] "<< job.m_cmd << "&"<< endl;
        ///TODO: remove the last space!!!
        i++;
    }
}

JobsList::JobEntry * JobsList::getJobById(int jobId){
  for(auto& job : m_list){
        if (job.m_id == jobId) {
            return &job;
        }
    }
    return nullptr;
}

void JobsList::removeFinishedJobs() {
    if (m_list.empty()) {
        max_id = 0;
        return;
    }
    int max=0;
    SmallShell &shell = SmallShell::getInstance();
    for (auto it = m_list.begin(); it != m_list.end(); ++it) {
      auto job = *it;
      int status;
      int ret_wait = waitpid(job.m_pid, &status, WNOHANG);
      if (ret_wait == job.m_pid || ret_wait == -1) {
        m_list.erase(it);
        --it;
         }
         else if(job.m_id>max){
          max= job.m_id;
         }
        }
        max_id = max;
    }

 int JobsList::getMaxId(){
   return max_id;
 }


 JobsCommand::JobsCommand(const char* cmd_line): BuiltInCommand(cmd_line){}
 void JobsCommand::execute(){
    SmallShell& smash = SmallShell::getInstance();
    smash.getJobs()->printJobsList();
  
 }
  void JobsList::killAllJobs(){
    cout<<"smash: sending SIGKILL signal to " << m_list.size()<< " jobs:"<<endl;
    removeFinishedJobs();
    for (JobEntry element : m_list) {
      cout<<element.m_pid<< ": "<<element.m_cmd << "&"<<endl;//remove space
      kill(element.m_pid, SIGKILL);
    }
  }

   void JobsList::sigJobById(int jobId, int signum){
    JobEntry *job = getJobById(jobId);
    if(!job){
      cerr << "smash error: kill: job-id " << jobId << " does not exist" << endl;
      return;
    }
    if (kill(job->m_pid, signum) == SYS_FAIL) {
                perror("smash error: kill failed");
                return;
            }
             cout << "signal number " << signum << " was sent to pid " << job->m_pid << endl;
   }
  //JobsList(){}

  //-------------------------------------ForeGround-------------------------------------
    ForegroundCommand::ForegroundCommand(const char* cmd_line, JobsList* jobs):BuiltInCommand(cmd_line){}
    void ForegroundCommand::execute(){
       int numArgs;
       char **args = getArgs(this->m_cmd_line, &numArgs);
       if(numArgs > 2){
         cerr << "smash error: fg: invalid arguments" << endl;
        return;
       }


        SmallShell& smash = SmallShell::getInstance();
        int job_id;
        if(numArgs == 1){
          job_id = smash.getJobs()->getMaxId();
        }
        else try {
            if (!is_number(args[1]))
                throw exception();
            job_id = stoi(args[1]);
        } catch (exception &) {
            cerr << "smash error: fg: invalid arguments" << endl;
            //free_args(args, num_of_args);
            return;
        }
        JobsList::JobEntry *job = smash.getJobs()->getJobById(job_id);
        if(!job){
          cerr << "smash error: fg: job-id " << job_id << " does not exist" << endl;
          return;
        }
        if (job_id >= 0 && job) {
            int job_pid = job->m_pid;
            if (job->m_isStopped) {
                if (kill(job_pid, SIGCONT) == SYS_FAIL) {
                    perror("smash error: kill failed");
                    //free_args(args, num_of_args);
                    ///TODO: check free
                    return;
                }
            }
            
            int status;
            cout << job->m_cmd << " : " << job_pid << endl;
            smash.m_pid_fg = job_id;
            smash.getJobs()->removeJobById(job_id);
            if (waitpid(job_pid, &status, WUNTRACED) == SYS_FAIL) {
                perror("smash error: waitpid failed");
                //free_args(args, num_of_args);
                return;
            }



    }
    }




  //-------------------------------------Quit-------------------------------------
  QuitCommand::QuitCommand(const char* cmd_line, JobsList* jobs):BuiltInCommand(cmd_line), m_jobs(jobs){}
 void  QuitCommand::execute(){
  int numArgs=0;
  char** args = getArgs(this->m_cmd_line, &numArgs);
    if(numArgs >1 && string(args[1]) == "kill"){
      cout<<"killing.....";
       SmallShell& smash = SmallShell::getInstance();
       smash.getJobs()->killAllJobs();
    }
    exit(0);
 }

  //-------------------------------------Kill-------------------------------------
  KillCommand::KillCommand(const char* cmd_line, JobsList* jobs): BuiltInCommand(cmd_line), m_jobs(jobs){}
  void KillCommand::execute(){
    int num_of_args;
    char **args = getArgs(this->m_cmd_line, &num_of_args);
    
    if (num_of_args != 3) {
        cerr << "smash error: kill: invalid arguments" << endl;
    } else {

        int signum;
        int job_id;
        try {
            // Check for a valid job-id
            if (!is_number(args[2]))
                throw exception();
            char first_char = string(args[1]).at(0);
            char minus = '-';
            if (first_char != minus)
                throw exception();
            job_id = stoi(args[2]);

            // Check for a valid signal number

            if (!is_number(string(args[1]).erase(0, 1)))
                throw exception();
            signum = stoi(string(args[1]).erase(0, 1));
        } catch (exception &) {
            cerr << "smash error: kill: invalid arguments" << endl;
            //free_args(args, num_of_args);
            return;
        }
         SmallShell &shell = SmallShell::getInstance();
         shell.getJobs()->sigJobById(job_id, signum);
  }
  }


//-------------------------------------Command-------------------------------------

Command::Command(const char* cmd_line) : m_cmd_line(cmd_line) {}

Command::~Command() {
  m_cmd_line = nullptr;
}

const char* Command::gedCmdLine(){
  return this->m_cmd_line;
}

//-------------------------------------BuiltInCommand-------------------------------------

BuiltInCommand::BuiltInCommand(const char* cmd_line) : Command::Command(cmd_line) {}


//-------------------------------------ChangePromptCommand-------------------------------------

ChangePromptCommand::ChangePromptCommand(const char* cmd_line) : BuiltInCommand::BuiltInCommand(cmd_line) {}

ChangePromptCommand::~ChangePromptCommand() {}

void ChangePromptCommand::execute() {
  ///TODO: NOT SUPPOSED TO CHANGE ERROR MESSAGES
  int numArgs = 0;
  char** args = getArgs(this->m_cmd_line, &numArgs);
  SmallShell& smash = SmallShell::getInstance();
  if (numArgs == 1) {
    smash.chngPrompt();
  }
  else {
    smash.chngPrompt(string(args[1]));
  }
  free(args);
}

//-------------------------------------ShowPidCommand-------------------------------------

ShowPidCommand::ShowPidCommand(const char* cmd_line) : BuiltInCommand(cmd_line) {}

void ShowPidCommand::execute() {
  SmallShell& smash = SmallShell::getInstance();
  cout << "smash pid is " << smash.m_pid << endl;
}

//-------------------------------------GetCurrDirCommand-------------------------------------

GetCurrDirCommand::GetCurrDirCommand(const char* cmd_line) : BuiltInCommand(cmd_line) {}

void GetCurrDirCommand::execute() {
  SmallShell& smash = SmallShell::getInstance();
  if(smash.getCurrDir() == nullptr) {
    firstUpdateCurrDir();
  }
  cout << string(smash.getCurrDir()) << endl;
}

//-------------------------------------ChangeDirCommand-------------------------------------

ChangeDirCommand::ChangeDirCommand(const char* cmd_line, char** plastPwd) : BuiltInCommand(cmd_line), m_plastPwd(plastPwd) {}

///TODO: IF WANT TO MAKE THINGS MORE EFFICIENT - TRY TO SPLICE TOGETHER CURRDIR INSTEAD OF USING SYSCALL
void ChangeDirCommand::execute() {
  SmallShell& smash = SmallShell::getInstance();
  if(smash.getCurrDir() == nullptr) {
    firstUpdateCurrDir();
  }
  int numArgs = 0;
  char** args = getArgs(this->m_cmd_line, &numArgs);
  if (numArgs > 2) {
    perror("smash error: cd: too many arguments");
  }
  else if (*m_plastPwd == nullptr && string(args[1]) == "-") {
    perror("smash error: cd: OLDPWD not set");
  }
  else if (string(args[1]) == "-") {
    if (chdir(*m_plastPwd) != 0) {
      perror("smash error: chdir failed");
    }
    //switch current and previous directories
    char* temp = smash.getCurrDir();
    smash.setCurrDir(smash.getPrevDir());
    smash.setPrevDir(temp);
    return;
  }
  if (chdir(args[1]) != 0) {
    perror("smash error: chdir failed");
  }
  //If the given "path" is to go up, remove the last part of the current path
  if (string(args[1]) == "..") {
    smash.setPrevDir(smash.getCurrDir());
    smash.setCurrDir(goUp(smash.getCurrDir()));
    return;
  }
  //If the new path is the full path, set currDir equal to it
  
  if (checkFullPath(smash.getCurrDir(), args[1])) {
    smash.setPrevDir(smash.getCurrDir());
    smash.setCurrDir(args[1]);
  }
  //If not, append the new folder to the end of the current path
  else {
    smash.setPrevDir(smash.getCurrDir());
    //Figure out how to move the string into a char without allocating memory here and not being able to delete it
    smash.setCurrDir(smash.getCurrDir(), args[1]);
  }
}

//-------------------------------------ExternalCommand-------------------------------------
ExternalCommand::ExternalCommand(const char* cmd_line) : Command(cmd_line) {}

void ExternalCommand::execute() {
  int numArgs = 0;
  cout<<"414";
  char** args = getArgs(this->m_cmd_line, &numArgs);
  bool isComplex = string(this->m_cmd_line).find("*") != string::npos || string(this->m_cmd_line).find("?")!= string::npos;
  if (isComplex) {
    cout<<"417";
    execl("/bin/bash", "-c", "complex-external-command", args, (char*)NULL );
  }
  else {
    string command = string(args[0]);
    cout<<"412";
    execvp(command.c_str(), args);
  }
  free(args);//never get to here
}
