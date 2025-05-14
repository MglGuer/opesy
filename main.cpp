#include <iostream>
#include <windows.h>
#include <thread>
using namespace std;

void asciiHeader();
void displayHeader();
void enterMainLoop();

void asciiHeader(){

}

void displayHeader(){
    HANDLE h= GetStdHandle(STD_OUTPUT_HANDLE);

    asciiHeader();
    SetConsoleTextAttribute(h,10);
    cout << "Hello, Welcome to CSOPESY commandline!\n";
    SetConsoleTextAttribute(h,14);
    cout << "Type 'exit' to quit, 'clear' to clear the screen\n";
    SetConsoleTextAttribute(h,7);
}

void enterMainLoop(){
    string command;

    displayHeader();
    
    while(true){
        cout << "Enter command: ";
        getline(cin, command);

        
    }
}

int main() {

    enterMainLoop();
    return 0;
    
}