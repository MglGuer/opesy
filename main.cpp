#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <stdlib.h>
#include <windows.h>
#include <ctime>
#include <thread>
#include <mutex>
#include <queue>
#include <algorithm> // for sort
#include <filesystem>

#define MAX_CORES 4
#define MAX_SCREENS 10
#define NUM_COMMANDS_PER_PROCESS 100

using namespace std;


std::mutex queueMutex;
std::mutex mutx;  

bool isRunning = true;

std::vector<Screen> screenList; //global vector for list of screens
std::vector<Screen> finishedProcess;
std::vector<std::thread> coreList;

Screen curScreen;

string getTimestamp(){
    time_t timestamp; //make a time_t variable
    time(&timestamp); //gets the current time
    char buffer[30]; //array to store new time format
    tm* timeinfo = localtime(&timestamp); 
    strftime(buffer, sizeof(buffer), "%m/%d/%Y, %I:%M:%S %p", timeinfo); //changes time format
    return buffer;
}


// class that includes screen details
struct Screen{
    string screenName;
    int curInstruction;
    int totalInstruction;
    string timeCreated;

};

class Process{
    private:
        string name;
        int remainingInstructions;
        int totalInstructions = NUM_COMMANDS_PER_PROCESS;
        int id;

    public:
    // Constructor
        Process(const std::string& processName, int processId, int numInstructions): 
            name(processName), id(processId), totalInstructions(numInstructions), remainingInstructions(numInstructions) {}
    
    // print instruction where the process creates a new file and prints required string in file
    void printInstruction(){
        
        string fileType = ".txt";
        string processFile = name + fileType;      // using process name as filename (it should be smth like process01, process02, etc etc)

         
        std::ofstream newFile(processFile);
        /*if(newFile.good()){
            DeleteFile(processFile);
        }*/
        
        if(newFile.is_open()){
            // printing for 100 lines
            for(int i=0; i<NUM_COMMANDS_PER_PROCESS; i++){
                    newFile << "Hello world from " << name;
                }
            newFile.close();
        }
        else{
            cout << "Failed to create the file: " << processFile << endl;
        }
        
        
        
    }

    //void addInstruction(){
    //}
    
    // Execute one instruction of the process
    void executeInstruction() {
        
        if (remainingInstructions > 0) {
            printInstruction();
            remainingInstructions--;
        } else {
            //add the process to the finishedProcess vector
        }
    }

    // Get the remaining number of instructions
    int getRemainingInstructions() const {
        return remainingInstructions;
    }

    // Check if the process has finished
    bool hasFinished() const {
        return remainingInstructions == 0;
    }

    /*
    string startTime;
    string endTime;
    int coreAssigned;

    int burstTime;
    bool isFinished;

    string fileName;
    */
    
};

// FCFS scheduler template provided by Doc Neil's notes
class FCFSScheduler {
    private:
        int numCores;
        std::vector<std::vector<Process>> processQueues; // ready queue for fcfs

    public:
        FCFSScheduler(int cores) : numCores(cores){}

    // Add a process to the scheduler
    void addProcess(const Process& process, int core = MAX_CORES) {
        if (core >= 0 && core < numCores) {
            processQueues[core].push_back(process);
        } else {
            std::cerr << "Invalid core specified for process addition.\n";
        }
    }

    // Sort the process queues based on remaining instructions (FCFS)
    // void sortProcessQueues() {
    //     for (auto& queue : processQueues) {
    //         std::sort(queue.begin(), queue.end(), [](const Process& a, const Process& b) {
    //             return a.getRemainingInstructions() > b.getRemainingInstructions();
    //         });
    //     }
    // }

    // Run the scheduler
    void runScheduler() {
        for(int i=1; i <= MAX_SCREENS; i++){
            string processname = "screen_";
            if(i<10)
                processname+="0";
            processname+=i+1;
            Process newProcess(processname, i, NUM_COMMANDS_PER_PROCESS);
            addProcess(newProcess);
        }
        while (!processQueues[0].empty()) { // This condition likely needs refinement for multiple cores
            for (int core = 0; core < numCores; ++core) {
                if (!processQueues[core].empty()) {
                    Process currentProcess = processQueues[core].back(); // Likely intended to be front() for FCFS
                    processQueues[core].pop_back(); // Likely intended to be pop_front()

                    while (!currentProcess.hasFinished()) {
                        currentProcess.executeInstruction();
                    }

                    //std::cout << "Process " << currentProcess.getRemainingInstructions() << " completed on Core " << core + 1 << ".\n";
                }
            }
        }
    }
};

//changes the font color of the text in the terminal
void setColor(int color){
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), color);
}

//opens and reads a file containing ASCII art for the main menu title
void printASCII(string fileName){
    string line = ""; //line being read
    ifstream inFile; //input file stream
    inFile.open(fileName); //opens the file
    if(inFile.is_open()){
        while(getline(inFile, line))
        {
            cout << line << endl; //prints file content
        }
    }
    else{
        cout << "File failed to load. " << endl;
    }
    inFile.close();
}

void initialize(){
    cout << "Initialize command recognized. Doing something.\n\n";
}


void createScreen (string &screenName){
    system("CLS");
    
    Screen newScreen; //initialize a new screen
    newScreen.screenName = screenName;
    newScreen.totalInstruction = rand()%50+1; //placeholder total instructions
    newScreen.curInstruction = rand()%(newScreen.totalInstruction-1)+1; //placeholder currnet instruction, guaranteed to be less than total instruction
    
    newScreen.timeCreated = getTimestamp();

    screenList.emplace_back(newScreen);     //adds screen to vector for storage
    curScreen = screenList.back(); //gets the most recently added screen for display
    cout << "Current screen: " << curScreen.screenName << endl;
    cout << "Running instruction: " << curScreen.curInstruction << " out of " << curScreen.totalInstruction << endl;
    cout << "Time Created: " << curScreen.timeCreated << endl << endl;
}



void displayScreens(){
    //TODO: screen -ls

    //displayResourceUsage()

    cout << "----------------------" << endl;
    cout << " Running Processes:   " << endl;
    // for(auto& scr : screenList){
    // process name, (timestamp when process started execution), core the process is assigned to, current instruction/total instruction
    // }

    cout << "Finished Processes: " << endl;
    //if finisheprocess vector isnt empty
    //lopp through the vector and display
    //process name, (timestamp when process finished), "Finished", current instruction/total instruction
    cout << "----------------------" << endl;
}

void screen(string &screenCommand){
    istringstream iss(screenCommand); //splits input separated by whitespaces
    string screen, option, screenName;
    iss >> screen >> option >> screenName; //stores each part of the input in its own variable

    if(option == "-s" && !screenName.empty()){
        createScreen(screenName);
    }

    else if(option == "-r" && !screenName.empty()){ 
        bool found = false;
        for(auto& scr : screenList){
            if(scr.screenName == screenName){
            system("CLS");
            curScreen = scr;
            cout << "Screen: " << curScreen.screenName << endl;
            cout << "Running instruction: " << curScreen.curInstruction << " out of " << curScreen.totalInstruction << endl;
            cout << "Time Created: " << curScreen.timeCreated << endl << endl;
            found = true;
            break;
        }
        
    }
    if(!found){
            cout << "Screen \"" << screenName << "\" not found." << endl << endl;
            return;
        }
    }

    else if(option == "-ls"){
        displayScreens();
        return;
    }


    string screenInput;

    while(screenInput != "exit"){
        cout << "Enter a command, or type exit to return to the main menu: ";
        getline(cin, screenInput);
        if(screenInput != "exit"){
            cout << "Sorry, that command does not work right now. Only 'exit' works at the moment." << endl << endl;
        }
        else{
            system("CLS");
            printASCII("ascii.txt");
        }
    }
}



// void roundrobin(){
//  TODO: round robin scheduler
// } 

void schedulertest(){
    cout << "Starting scheduler..." << endl;
    FCFSScheduler fcfs(MAX_CORES);
    std::thread runscheduler(&FCFSScheduler::runScheduler, &fcfs);
    runscheduler.detach();
    /*if(scheduler == "fcfs"){
        std::thread scheduler(fcfs);
        scheduler.detach(); (or join, im not sure)
    }
    else if(scheduler == "rr"){
        roundrobin();
    }
    */
    std::cout.flush();
}

void schedulerstop(){
    cout << "Scheduler-stop command recognized. Doing something.\n\n";
}

void reportutil(){
    cout << "Report-util command recognized. Doing something.\n\n";
}

void intro(){
    string fileName = "ascii.txt";
    
    setColor(10);
    std::cout << "Hello, Welcome to CSOPESY commandline!"<< std::endl;
    setColor(14);
    std::cout<<"Type 'exit' to quit, 'clear' to clear the screen."<< std::endl;
}

void menu(){
    string fileName = "ascii.txt";
    string input;
    printASCII(fileName);
    while (isRunning){
        intro();
        setColor(7);
        cout << "Enter a command: ";
        string command;
        getline(cin >> ws, command); //changed to getline to read input separated by whitespace
         
        if(command == "exit"){
            isRunning = false;
            cout << "Thank you for using the program";
        }
        else if (command == "clear"){
            system("CLS"); //clears the system
        }
        else if(command == "initialize"){
            initialize();
        }
        else if(command.rfind("screen",0) == 0){
            screen(command);
        }
        else if(command == "scheduler-test"){
            std::thread scheduler(schedulertest);
            scheduler.detach();
        }
        else if(command == "scheduler-stop"){
            schedulerstop();
        }
        else if(command == "report-util"){
            reportutil();
        }
        else{
            cout << "Command not recognized, please try again." << endl;
        }
    }
    std::cout.flush();    
}

int main(){
    std::thread mainMenu(menu);
    mainMenu.join();
    return 0;
}