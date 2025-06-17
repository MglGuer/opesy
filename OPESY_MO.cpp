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

#define MAX_CORES 4
#define MAX_SCREENS 10

using namespace std;


std::mutex queueMutex;
std::mutex mutx;  

bool isRunning = true;

// class that includes screen details
struct Screen{
    string screenName;
    int curInstruction;
    int totalInstruction;
    string timeCreated;

};

struct Process{
    string processName;
    int curInstruction;
    int totalInstruction;

    string startTime;
    string endTime;
    int coreAssigned;

    int burstTime;
    bool isFinished;

    string fileName;
};

std::vector<Screen> screenList; //global vector for list of screens
std::queue<Screen*> readyQueue;

Screen curScreen;

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
    time_t timestamp; //make a time_t variable
    time(&timestamp); //gets the current time

    Screen newScreen; //initialize a new screen
    newScreen.screenName = screenName;
    newScreen.totalInstruction = rand()%50+1; //placeholder total instructions
    newScreen.curInstruction = rand()%(newScreen.totalInstruction-1)+1; //placeholder currnet instruction, guaranteed to be less than total instruction
    
    char buffer[30]; //array to store new time format
    tm* timeinfo = localtime(&timestamp); 
    strftime(buffer, sizeof(buffer), "%m/%d/%Y, %I:%M:%S %p", timeinfo); //changes time format
    newScreen.timeCreated = buffer;

    screenList.emplace_back(newScreen);     //adds screen to vector for storage
    curScreen = screenList.back(); //gets the most recently added screen for display
    cout << "Current screen: " << curScreen.screenName << endl;
    cout << "Running instruction: " << curScreen.curInstruction << " out of " << curScreen.totalInstruction << endl;
    cout << "Time Created: " << curScreen.timeCreated << endl << endl;
}

void displayScreens(){
    //TODO: screen -ls
    cout << "+----------------+------------------------+---------------+------------+" << endl;
    cout << "|  Process Name  |        Timestamp       |      Core     |  Progress  |" << endl;
    cout << "+----------------+------------------------+---------------+------------+" << endl;
    // for(auto& scr : screenList){
    // process name, timestamp when -ls was called, core the process is assigned to, current instruction/total instruction
    // }
    cout << "+----------------+------------------------+---------------+------------+" << endl;
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
void fcfs(){
    //TODO: FCFS scheduler

}

// void roundrobin(){
//  TODO: round robin scheduler
// }

void schedulertest(){
    cout << "Starting scheduler..." << endl;
    std::thread schedulertest(fcfs);
    schedulertest.detach();
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