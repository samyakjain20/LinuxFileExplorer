
// file permissions
#include <bits/stdc++.h>
// #include <string>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h> // for ctime
#include <pwd.h> // for getpwuid
#include <errno.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#include <fstream> //for creating file
#include <iostream>
#include <sys/wait.h> //opening the file
#include <signal.h>

using namespace std;
/*** defines ***/
#define CTRL_KEY(k) ((k) & 0x1f)

/*** global variables ***/ 
string dirPath = ".";
vector<string> files;
stack<string> back, front;
int over = 0;
bool flag = false;
string cmd="";
string username = getlogin();

enum arrowKeys {
  ARROW_LEFT = 'a',
  ARROW_RIGHT = 'd',
  ARROW_UP = 'w',
  ARROW_DOWN = 's'
};

/*** data ***/
struct explorerConfig {
  int cx, cy;
  int screenrows;
  int screencols;
  struct termios orig_termios;
};
struct explorerConfig E;

/*** terminal ***/
void die(const char *s) {
  write(STDOUT_FILENO, "\x1b[2J", 4);
  write(STDOUT_FILENO, "\x1b[H", 3);
  perror(s);
  exit(1);
}
void disableRawMode() {
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1)
    die("tcsetattr");
}
void enableRawMode() {
  if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1) die("tcgetattr");
  atexit(disableRawMode);
  
  struct termios raw = E.orig_termios;
  raw.c_iflag &= ~(IXON); //disabling ctrl+S, ctrl+Q
  raw.c_lflag &= ~(ECHO | ICANON | ISIG); // ISIG diables ctrl+C, ctrl+Z
  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 1;

  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) die("tcsetattr");
}
char getletter() {
  int nread;
  char c;
  while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
    if (nread == -1 && errno != EAGAIN) die("read");
  }
  // write(STDIN_FILENO, &c, 1);
  if (c == '\x1b') {
    char seq[3];
    if (read(STDIN_FILENO, &seq[0], 1) != 1) c= '\x1b';
    if (read(STDIN_FILENO, &seq[1], 1) != 1) c= '\x1b';
    if (seq[0] == '[') {
      // c = seq[1];

      switch (seq[1]) {
        case 'A': return ARROW_DOWN;
        case 'B': return ARROW_UP;
        case 'C': return ARROW_RIGHT;
        case 'D': return ARROW_LEFT;
      }
    }
    return '\x1b';
  } else {
    return c;
  }
}
string getFileName(string filePath){
  int n = filePath.size(), j = n-1;
  while(j>=0 && filePath[j--] != '/');
  j++;
  return filePath.substr(j+1,n-j-1);
}

string getFileDetails(string file){
	struct stat sfile;
	int a = stat(file.c_str(), &sfile);
  
	string s;
	s += ((sfile.st_mode & S_IFDIR) ? "d" : "-");
	s += ((sfile.st_mode & S_IRUSR) ? "r" : "-"); // owner
	s += ((sfile.st_mode & S_IWUSR) ? "w" : "-");
	s += ((sfile.st_mode & S_IXUSR) ? "x" : "-");
	s += ((sfile.st_mode & S_IRGRP) ? "r" : "-"); // group
	s += ((sfile.st_mode & S_IWGRP) ? "w" : "-");
	s += ((sfile.st_mode & S_IXGRP) ? "x" : "-");
	s += ((sfile.st_mode & S_IROTH) ? "r" : "-"); // others
	s += ((sfile.st_mode & S_IWOTH) ? "w" : "-");
	s += ((sfile.st_mode & S_IXOTH) ? "x" : "-");
	s += "\t";
	
  long long int ctr = 0, size=sfile.st_size;
	while(size > 999){
		ctr++;
		size = size / 1000;
	}
	string fileSize = to_string(size);
	if(ctr == 0) fileSize += "B";
	else if(ctr == 1) fileSize += "KB";
	else if(ctr == 2) fileSize += "MB";
	else if(ctr == 3) fileSize += "GB";
	else if(ctr == 4) fileSize += "TB";
	s += fileSize + "\t";
  
	struct passwd *username = getpwuid(sfile.st_uid);
  string usrname = username->pw_name;
	s += usrname + '\t';
	struct passwd *group = getpwuid(sfile.st_gid);
	string grp = group->pw_name;
	s += grp + '\t';
	
  string mod_time = ctime(&sfile.st_mtime);
	s += mod_time.substr(4, 12) + "\t" + getFileName(file);
  return s;
}

void getAbsolutePath(string &path){
  if(path[0] == '~'){
    path = "/home/" + username + path.substr(1, path.size()-1);
  }
  else if(path.substr(0, 5) != "/home")
    path = dirPath + '/' + path;
	char resolved_path[PATH_MAX];
  realpath(path.c_str(), resolved_path); 
  path = resolved_path;
  cout<<path<<" ;abs\n";
}

bool validPath(string &path){
  getAbsolutePath(path);
	struct stat sfile;
	int exist = stat(path.c_str(), &sfile);
  // cout<<path<<" "<<exist<<endl;
	if(exist == -1)
    return false;
  else 
    return true;  
}

vector<string> getArguments(string inpurArg){
  vector<string> args;
  int i=0, j=0, n = inpurArg.size();
  while(i<n){
    if(i == j && inpurArg[i] == ' ')
      j++;
    else if(inpurArg[i] == ' '){
      args.push_back(inpurArg.substr(j, i-j));
      j = i+1;
    }
    i++;
  }
  string directory_path = inpurArg.substr(j, i-j);
  if(!args.empty())
    getAbsolutePath(directory_path);
  args.push_back(directory_path);
  return args;
}

void open(string fileName){
  string curr = dirPath, details=getFileDetails(dirPath+'/'+fileName);
  switch (details[0]){
    case 'd':
      E.cy = 0;
      over = 0;
      back.push(dirPath);
      if(fileName == ".")
        return;
      else if(fileName == ".."){
        int j = curr.size()-1;
        while(curr[j]!='/')
          j--;
        dirPath = dirPath.substr(0, j);
      }else{
        dirPath += '/' + fileName;
      }
      break;
    case '-':
      int pid = fork();
      if (pid == 0) {
        string fileOpening = dirPath + '/' +fileName;
          execl("/usr/bin/xdg-open", "xdg-open", fileOpening.c_str(), NULL);
          exit(0);
      }else{
        int wval, status = 0;
        while((wval = waitpid(pid, &status, 0)) != pid){
          if(wval == -1) exit(1);
        }
      }
      break;
  }
}

bool searchFile(string dir_to_search, string &fileName){
  DIR *dir;
	struct dirent *sd;
	dir = opendir(dir_to_search.c_str());
  cout<<dir_to_search<<endl;
	if(dir == NULL)
    return false;
  vector<string> directories;
	while( (sd=readdir(dir)) != NULL ){
    string currFile = sd->d_name, fileDetails = getFileDetails(dir_to_search + '/'+sd->d_name); 
    if(currFile == fileName ){
      fileName = dir_to_search + '/'+sd->d_name;
      return true;
    }
    else if(currFile == ".." || currFile == ".")
      continue;
    struct stat sfile;
    stat(currFile.c_str(), &sfile);
    if(fileDetails[0] == 'd' && fileDetails[7] == 'r')
    	directories.push_back(dir_to_search+"/"+ currFile);
  }
	closedir(dir);
  for(int i=0; i<directories.size(); i++)
    if(searchFile(directories[i], fileName))
      return true;
  return false;
}

void createFile(string dir_name){
  fstream output_fstream;
  output_fstream.open(dir_name, std::ios_base::out);
}
void copyFileDir(string filePath, string destDir, int &err){
  int errFlag = 0;
  if(!validPath(filePath)){
    // cout<<filePath<<endl;
    cmd = "Invalid Path";
    return;
  }
  string line, fileDetails = getFileDetails(filePath);
  string file = getFileName(filePath);
  cout<<fileDetails[0]<<"\n";
  if(fileDetails[0] == 'd'){
    string destRecurDir = destDir + '/' + file;
    if (mkdir(destRecurDir.c_str(), 0777) == -1) cmd = " Error Occured";

    DIR *dirToBeCopied;
    string currDir = filePath;
    dirToBeCopied = opendir(currDir.c_str());
    
    struct dirent *sd;
    while( (sd=readdir(dirToBeCopied)) != NULL ){
      string currFile = sd->d_name;
      if(currFile == ".." || currFile == ".")
        continue;
      else
        copyFileDir(filePath + '/' +currFile, destRecurDir, errFlag);
    }
  }
  else{
    string sourceFile = filePath, targetFile = destDir + '/' + getFileName(filePath);
    ifstream ini_file{ sourceFile }; // This is the original file
    ofstream out_file{ targetFile };
    // cout<<sourceFile<<" "<<targetFile<<" sdzcsdzcscx";
    if (ini_file && out_file)
      while (getline(ini_file, line)) 
        out_file << line << "\n";
    else errFlag = 1;

    // Closing file
    ini_file.close();
    out_file.close();
  }
  if(errFlag) err = 1;
}

void deleteDirectory(string currPath, string directoryPath, int &err){
  int errFlag = 0;
  if(searchFile(currPath, directoryPath)){
    cout<<currPath<<" found "<<directoryPath<<endl;
    DIR *dirToBeDeleted;
    dirToBeDeleted = opendir(directoryPath.c_str());
    
    struct dirent *sd;
    while( (sd=readdir(dirToBeDeleted)) != NULL ){
      string currFile = sd->d_name, fileDetails = getFileDetails(directoryPath + '/'+sd->d_name);
      if(currFile == ".." || currFile == ".")
        continue;
      if(fileDetails[0] == 'd'){
        cout<<sd->d_name<<" Dir Found in dlete_dir\n";
        string recurDir = directoryPath + '/' + sd->d_name;
        deleteDirectory(directoryPath, sd->d_name, errFlag);
      }
      else if(remove((directoryPath+ '/'+sd->d_name).c_str()) == -1) errFlag = 1;
    }
    if(rmdir(directoryPath.c_str()) == -1) errFlag = 2;
  }
  if(errFlag) err = errFlag;
}

void processLetter() {
  int nread;

  char c = getletter();
  if(!flag)
    switch (c) {
      case ('q'):
        write(STDOUT_FILENO, "\x1b[2J", 4);
        write(STDOUT_FILENO, "\x1b[H", 3);
        exit(0);
        break;
      case ('h'):
        while(!front.empty())
          front.pop();
        back.push(dirPath);
        dirPath = "/home/" + username;
        break;
      case 10:
        open(files[over + E.cy]);
        while(!front.empty())
          front.pop();
        break;
      case 127:
        while(!front.empty())
          front.pop();
        back.push(dirPath);
        open("..");
        break;
      case 58:
        flag = !flag;
        break;
      case ARROW_LEFT:
        if(!back.empty()){
          front.push(dirPath);
          dirPath = back.top();
          back.pop();
        }
        break;
      case ARROW_RIGHT:
        if(!front.empty()){
          back.push(dirPath);
          dirPath = front.top();
          front.pop();
        }
        break;
      case ARROW_DOWN:
        if(E.cy == 0 && over > 0)
          over--;
        else if(E.cy > 0)
          E.cy--;
        break;
      case ARROW_UP:
        if(E.cy == (E.screenrows-1) && files.size() > (over+E.cy))
          over++;
        else if ((over + E.cy) < (files.size() - 1))
          E.cy++;
        break;
    }
  else{
    if(c == 27){
      flag = !flag;
      E.cx = E.cy = 0;
    }
    cmd += c;
    switch (c) {
      case 127:
        cmd = cmd.substr(0, cmd.size()-2);
        break;
      case 10:
        string keyWord = "", inputArg="";
        int j = 0;
        while(j<cmd.size() && cmd[j] != ' ')
          j++;
        if(j < cmd.size()){
          keyWord = cmd.substr(0, j);
          inputArg = cmd.substr(j+1, cmd.size()-j-2);
        }
        else keyWord = cmd.substr(0, j-1);
        cmd = "";
        if(keyWord == "goto"){
          if(!validPath(inputArg)){
            cmd = "Invlaid Path";
            break;
          }
          cmd = "";
          dirPath = inputArg;
        }
        else if(keyWord == "search"){
          if(searchFile(dirPath, inputArg)) cmd = "True";
          else cmd = "False";
        }
        else if(keyWord == "create_dir"){
          vector<string> filesToBeCopied = getArguments(inputArg);
          int n = filesToBeCopied.size(), flag = 0;
          if(n>1)
            for(int i=0; i<n-1; i++){
              string dir_name = filesToBeCopied[n-1]+'/'+filesToBeCopied[i];
              if (mkdir(dir_name.c_str(), 0777) == -1) cmd = " Error Occured: "+dir_name;
              else cmd = "All directory created";
            }
          else{
            string dir_name = dirPath+'/'+filesToBeCopied[0];
            if (mkdir(dir_name.c_str(), 0777) == -1) cmd = " Error Occured: "+dir_name;
            else cmd = "All directory created";
          }
        }
        else if(keyWord == "create_file"){
          vector<string> filesToBeCopied = getArguments(inputArg);
          int n = filesToBeCopied.size();
          if(n>1)
            for(int i=0; i<n-1; i++)
              createFile(filesToBeCopied[n-1]+'/'+filesToBeCopied[i]);
          else
              createFile(dirPath+'/'+filesToBeCopied[0]);
        }
        else if(keyWord == "rename"){
          int k = 0, i=0;
          while(k<inputArg.size() && inputArg[k] != ' ')
            k++;
          string oldName = inputArg.substr(0, k), newName = inputArg.substr(k+1, inputArg.size()-k-1);
          while(newName[i] != '/' && i<newName.size())
            i++;

          if(i<newName.size())
            cmd = "Name cannot have '/'";
          else if(validPath(oldName)){
            int j=oldName.size() - 1;
            while(oldName[j] != '/' && j>=0)
              j--;
            string path = oldName.substr(0, j);
            cout<<path<<j<<" "<<(path+ '/' + newName)<<endl;
            rename(oldName.c_str(), (path+ '/' + newName).c_str());
          }
        }
        // rename xyz/temp2t2 t2
        else if(keyWord == "delete_file"){
          vector<string> filesToBeDeleted = getArguments(inputArg);
          int n = filesToBeDeleted.size();
          for(int i=0; i<n; i++){
            if(searchFile(dirPath, filesToBeDeleted[i]))

              if(remove(filesToBeDeleted[i].c_str()) == -1) cmd = "Error Occured";
              else cmd = "Deleted";
          }
        }
        else if(keyWord == "delete_dir"){
          vector<string> filesToBeDeleted = getArguments(inputArg);
          int n = filesToBeDeleted.size(), errInAll = 0;
          for(int i=0; i<n; i++){
            int err = 0;
            deleteDirectory(dirPath, filesToBeDeleted[i], err);
            if(err == 1 || err == 2)
              errInAll = err;
          }
          if(errInAll == 1)
            cmd = "Error while deleting a file inside a dir";
          else if(errInAll == 2)
            cmd = "Error while deleting a dir";
        }
        else if (keyWord == "move"){
          vector<string> filesToBeCopied = getArguments(inputArg);
          int n = filesToBeCopied.size(), err = 0;
          string destDir = filesToBeCopied[n - 1];
          
          if(validPath(destDir))
            for(int i=0; i<n-1; i++){
              getAbsolutePath(filesToBeCopied[i]);
              if(filesToBeCopied[i] == (destDir + '/' + getFileName(filesToBeCopied[i])))
                continue;
              string oldName = filesToBeCopied[i];          
              if(!validPath(oldName)){
                cmd = 4;
                continue;
              }    
              string fileDetails =  getFileDetails(oldName);
              copyFileDir(filesToBeCopied[i], destDir, err);
              if(fileDetails[0] == 'd')
                deleteDirectory(dirPath, getFileName(filesToBeCopied[i]), err);
              else if(remove(oldName.c_str()) == -1) err = 1;
            }
          else
            err = 5;
          switch(err){
            case 4:
              cmd = "File/s doesn't exists"; 
              break;
            case 5:
              cmd == "Invalid Dest Path"; 
              break;
            case 0:
              cmd = "Moving Done";
              break;
            defaut:
              cmd = "Error Occured";
              break;
          }
        }
        else if(keyWord == "copy"){
          vector<string> filesToBeCopied = getArguments(inputArg);
          int n = filesToBeCopied.size(), err = 0;
          string destDir = filesToBeCopied[n - 1], srcPath = dirPath;
          if(validPath(destDir))
            for(int i=0; i<n-1; i++)
              copyFileDir(filesToBeCopied[i],  destDir, err);
          else
            cmd = "Invalid Dest Path";
        }
        else if(keyWord == "quit"){
          write(STDOUT_FILENO, "\33c", 4);
          write(STDOUT_FILENO, "\x1b[2J", 4);
          write(STDOUT_FILENO, "\x1b[H", 3);
          exit(0);
        }
        break;
    }
  }
}

int getCursorPosition(int *rows, int *cols) {
  char buf[32];
  unsigned int i = 0;

  if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return -1;
  while (i < sizeof(buf) - 1) {
    if (read(STDIN_FILENO, &buf[i], 1) != 1) break;
    if (buf[i] == 'R') break;
    i++;
  }
  buf[i] = '\0';
  printf("\r\n&buf[1]: '%s'\r\n", &buf[1]);

  processLetter();
  if (buf[0] != '\x1b' || buf[1] != '[') return -1;
  if (sscanf(&buf[2], "%d;%d", rows, cols) != 2) return -1;
  return 0;
}
int getWindowSize(int *rows, int *cols) {
  struct winsize ws;
  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
    if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) return -1;
    return getCursorPosition(rows, cols);
  } else {
    *cols = ws.ws_col;
    *rows = ws.ws_row;
    return 0;
  }
}

void printRows() {
  chdir(dirPath.c_str());
  DIR *dir;
	struct dirent *sd;
	char resolved_path[PATH_MAX];
  if(dirPath[0] != '/'){
  	realpath(dirPath.c_str(), resolved_path); 
    dirPath = resolved_path;
  }
	dir = opendir(dirPath.c_str());
	if(dir == NULL){
		cout<<dirPath<<"\t"<<resolved_path<<"\tNo Directory found!\n";
    string homePath ="/home/" + username + "/"; 
    dir = opendir(homePath.c_str());
		// exit(1);
	}
  while(!files.empty())
    files.pop_back();
	while( (sd=readdir(dir)) != NULL )
		files.push_back(sd->d_name);
	sort(files.begin(), files.end());
	closedir(dir);

  int y;
  for (y = 0; y < E.screenrows; y++) {
    if(y<files.size()){
  		string s ="";
      // s += to_string(y)+'\t';
      s += getFileDetails(dirPath+'/'+files[y + over]);
      s = s.substr(0, min(E.screencols-12 ,(int) s.size()));
      write(STDOUT_FILENO, s.c_str(), s.size());  
    }
    write(STDOUT_FILENO, "\x1b[K", 3); // prints next line
    write(STDOUT_FILENO, "\r\n", 2);
  }
  write(STDOUT_FILENO, "\x1b[K", 3); // prints next line
  string cwd = "cwd: " + dirPath;
  write(STDOUT_FILENO, cwd.c_str(), cwd.size());
  write(STDOUT_FILENO, "\r\n", 2);
  if(!flag){
    string temp = "Normal Mode!";
    write(STDOUT_FILENO, temp.c_str(), 12);
  }
  else{
    string commandLine =  "Command Mode: " + cmd;
    write(STDOUT_FILENO, commandLine.c_str(), commandLine.size());
    E.cx = commandLine.size();
    E.cy = E.screenrows+1;
  }
}
void clearScreen() {
  
  // write(STDOUT_FILENO, "\x1b[?25l", 6); // flush
  write(STDOUT_FILENO, "\33c", 4);
  write(STDOUT_FILENO, "\x1b[2J", 4); // refresh
  write(STDOUT_FILENO, "\x1b[H", 3);
  printRows();

  /** To show moving cursor STARTS**/ 
  char buf[32];
  snprintf(buf, sizeof(buf), "\x1b[%d;%dH", E.cy + 1, E.cx + 1);
  write(STDOUT_FILENO,  buf, strlen(buf));
  /** To show moving cursor ENDS**/ 

  // write(STDOUT_FILENO, "\x1b[H", 3);
  write(STDOUT_FILENO, "\x1b[?25h", 6);
}

void startEditor() {
  E.cx = 0;
  E.cy = 0;

  if (getWindowSize(&E.screenrows, &E.screencols) == -1) die("getWindowSize");
  E.screenrows = E.screenrows - 2;
}

void resizeHandler(int dummy){
  if (getWindowSize(&E.screenrows, &E.screencols) == -1) die("getWindowSize");
  E.screenrows = E.screenrows - 2;
  over = 0;
  clearScreen();
  // printRows();
}

int main() {
  enableRawMode();
  startEditor();

  char c;
  while (1) {
    signal(SIGWINCH, resizeHandler);
    clearScreen();
    processLetter();
  }
  
  return 0;
}
