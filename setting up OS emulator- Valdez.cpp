#include <iostream>
#include <string>
using namespace std;

int main(){

    std:: string opt = "";

    while(true){
    
        cout    << " ____  ____  ____  ____  _____ ____ ___  _" << endl
                << "/   _\\/ ___\\/  _ \\/  __\\/  __// ___\\  \\//" << endl
                << "|  /  |    \\| / \\||  \\/||  \\  |    \\\\  / " << endl
                << "|  \\__\\___ || \\_/||  __/|  /_ \\___ | / /  " << endl
                << "\\____/\\____/\\____/\\_/   \\____\\____//_/ " << endl;

        cout << "\033[1;32mHello, welcome to CSOPESY commandline!\033[0m\n" << endl;
        cout << "Type 'exit' to quit, 'clear' to clear the screen" << endl;
        cout << "Enter a command:" << endl;
        
        cin >> opt;

        if(opt == "exit"){
            break;
        }else if (opt == "clear"){
            system("cls");
        }else if(opt == "initialize" || "screen" || "scheduler-test" || "scheduler-stop" || "report-util"){
            system("cls");
            cout << opt << " command recognized. Doing something...\n" << endl;
        }else{
            cout << "Command unknown.\n" << endl;
        }
        
    }
    

}