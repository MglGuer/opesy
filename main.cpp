#include "libraries.h"

// class that includes screen details
class Screen{
    public:
        std::string screenName;
        int curInstruction;
        int totalInstruction;
        std::string timeCreated;
};

std::vector<Screen> screenList; //global vector for list of screens (c++ can't handle dynamic sized arrays)

//changes the font color of the text in the terminal
void setColor(int color){
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), color);
}

//opens and reads a file containing ASCII art for the main menu title
void printASCII(std::string fileName){
    std::string line = ""; //line being read
    std::ifstream inFile; //input file stream
    inFile.open(fileName); //opens the file
    if(inFile.is_open()){
        while(std::getline(inFile, line))
        {
            std::cout << line << std::endl; //prints file content
        }
    }
    else{
        std::cout << "File failed to load. " << std::endl;
    }
    inFile.close();
}

void initialize(){
    std::cout << "Initialize command recognized. Doing something.\n\n";
}

void createScreen (std::string &screenName){
    std::time_t timestamp; //make a time_t variable
    std::time(&timestamp); //gets the current time

    Screen newScreen; //initialize a new screen
    newScreen.screenName = screenName;
    newScreen.totalInstruction = std::rand()%50+1; //placeholder total instructions
    newScreen.curInstruction = std::rand()%(newScreen.totalInstruction-1)+1; //placeholder currnet instruction, guaranteed to be less than total instruction
    
    char buffer[30]; //array to store new time format
    std::tm* timeinfo = std::localtime(&timestamp); 
    std::strftime(buffer, sizeof(buffer), "%m/%d/%Y, %I:%M:%S %p", timeinfo); //changes time format
    newScreen.timeCreated = buffer;

    screenList.emplace_back(newScreen);       //adds screen to vector for storage
    
}

void screen(std::string &screenCommand){
    std::istringstream iss(screenCommand); //splits input separated by whitespaces
    std::string screen, option, screenName;
    iss >> screen >> option >> screenName; //stores each part of the input in its own variable

    if(option == "-s" && !screenName.empty()){
        createScreen(screenName);
        system("CLS");
        Screen& createdScreen = screenList.back(); //gets the most recently added screen for display
        std::cout << "Current screen: " << createdScreen.screenName << std::endl;
        std::cout << "Running instruction: " << createdScreen.curInstruction << " out of " << createdScreen.totalInstruction << std::endl;
        std::cout << "Time Created: " << createdScreen.timeCreated << std::endl << std::endl;
    }

    else if(option == "-r" && !screenName.empty()){
        //TODO screen -r  
        bool found = false;
        for(auto& scr : screenList){
            if(scr.screenName == screenName){
            system("CLS");
            std::cout << "Screen: " << scr.screenName << std::endl;
            std::cout << "Running instruction: " << scr.curInstruction << "out of" << scr.totalInstruction << std::endl;
            std::cout << "Time Created: " << scr.timeCreated << std::endl << std::endl;
            found = true;
            break;
        }
        if(!found){
            std::cout << "Screen \"" << screenName << "\" not found." << std::endl << std::endl;
        }
    }

    }


    std::string screenInput;

    while(screenInput != "exit"){
        std::cout << "Enter a command, or type exit to return to the main menu: ";
        std::getline(std::cin, screenInput);
        if(screenInput != "exit"){
            std::cout << "Sorry, that command does not work right now. Only 'exit' works at the moment." << std::endl << std::endl;
        }
        else{
            system("CLS");
        }
    }
}



void schedulertest(){
    std::cout << "Scheduler-test command recognized. Doing something.\n\n";
}

void schedulerstop(){
    std::cout << "Scheduler-stop command recognized. Doing something.\n\n";
}

void reportutil(){
    std::cout << "Report-util command recognized. Doing something.\n\n";
}

void intro(){
    std::string fileName = "ascii.txt";
    printASCII(fileName);
    setColor(10);
    std::cout << "Hello, Welcome to CSOPESY commandline!"<< std::endl;
    setColor(14);
    std::cout<<"Type 'exit' to quit, 'clear' to clear the screen."<< std::endl;
}

void menu(){
    std::string fileName = "ascii.txt";
    std::string input;

    while (true){
        intro();
        setColor(7);
        std::cout << "Enter a command: ";
        std::string command;
        std::getline(std::cin >> std::ws, command); //changed to getline to read input separated by whitespace

        if(command == "exit"){
            std::cout << "Thank you for using the program";
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