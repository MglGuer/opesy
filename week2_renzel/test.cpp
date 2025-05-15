#include <iostream>
#include <string>
#include <fstream>
#include <stdlib.h>
#include <windows.h>
using namespace std;

void setColor(int color){
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), color);
}

void printASCII(string fileName){
    string line = "";
    ifstream inFile;
    inFile.open(fileName);
    if(inFile.is_open()){
        while(getline(inFile, line))
        {
            cout << line << endl;
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

void screen(){
    cout << "Screen-command recognized. Doing something.\n\n";
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
    string command = "";
    
    intro();

    while (command != "exit"){
        setColor(7);
        cout << "Enter a command: ";
        cin >> command;
        if(command == "exit"){
            cout << "Thank you for using the program";
            exit(0);
        }
        else if (command == "clear"){
            system("CLS");
            intro();
        }
        else if(command == "initialize"){
            initialize();
        }
        else if(command == "screen"){
            screen();
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