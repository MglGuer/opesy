#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <stdlib.h>
#include <windows.h>
#include <ctime>
using namespace std;

// class that includes screen details
class Screen{
    public:
        string screenName;
        int curInstruction;
        int totalInstruction;
        string timeCreated;
};

std::vector<Screen> screenList; //global vector for list of screens (c++ can't handle dynamic sized arrays)

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

    screenList.emplace_back(newScreen); //adds screen to vector for storage
    
}

void screen(string &screenCommand){
    istringstream iss(screenCommand); //splits input separated by whitespaces
    string screen, option, screenName;
    iss >> screen >> option >> screenName; //stores each part of the input in its own variable

    if(option == "-s" && !screenName.empty()){
        createScreen(screenName);
        system("CLS");
        Screen& createdScreen = screenList.back(); //gets the most recently added screen for display
        cout << "Current screen: " << createdScreen.screenName << endl;
        cout << "Running instruction: " << createdScreen.curInstruction << " out of " << createdScreen.totalInstruction << endl;
        cout << "Time Created: " << createdScreen.timeCreated << endl << endl;
    }

    else if(option == "-r" && !screenName.empty()){
        //TODO screen -r

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
        }
    }
}

void schedulertest(){
    cout << "Scheduler-test command recognized. Doing something.\n\n";
}

void schedulerstop(){
    cout << "Scheduler-stop command recognized. Doing something.\n\n";
}

void reportutil(){
    cout << "Report-util command recognized. Doing something.\n\n";
}

void intro(){
    string fileName = "ascii.txt";
    printASCII(fileName);
    setColor(10);
    std::cout << "Hello, Welcome to CSOPESY commandline!"<< std::endl;
    setColor(14);
    std::cout<<"Type 'exit' to quit, 'clear' to clear the screen."<< std::endl;
}

void menu(){
    string fileName = "ascii.txt";
    string input;

    while (true){
        intro();
        setColor(7);
        cout << "Enter a command: ";
        string command;
        getline(cin >> ws, command); //changed to getline to read input separated by whitespace

        if(command == "exit"){
            cout << "Thank you for using the program";
            exit(0);
        }
        else if (command == "clear"){
            system("CLS"); //clears the system
            intro(); //reprints the main menu
        }
        else if(command == "initialize"){
            initialize();
        }
        else if(command.rfind("screen",0) == 0){
            screen(command);
        }
        else if(command == "scheduler-test"){
            schedulertest();
        }
        else if(command == "scheduler-stop"){
            schedulerstop();
        }
        else if(command == "report-util"){
            reportutil();
        }
    }
        
}

int main(){
    menu();
    return 0;
}