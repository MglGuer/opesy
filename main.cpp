#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>
#include <ctime>
#include <cstdlib>
#include <algorithm>
#include <iomanip>
#include <thread>
#include <mutex>
#include <chrono>
#include <queue>
#include <atomic>
#include <condition_variable>
#include <memory>
#include <bits/stdc++.h>
#include <iomanip> 

// Forward declarations
class Process;
class FCFSScheduler;


// Global variables
std::vector<std::unique_ptr<Process>> allProcesses;
std::vector<Process*> runningProcesses;
std::vector<Process*> finishedProcesses;
FCFSScheduler* scheduler = nullptr;
std::mutex processMutex;
std::atomic<bool> schedulerRunning{false};
std::atomic<int> processIdCounter{1};
std::atomic<long long> global_simulated_cycles{0}; // Global variable to track simulated cycles
std::



//FOR CONFIG.txt
int numCPU; //number of cores (between 1-128 inclusive)
std::string schedulerType; //scheduler type("fcfs" or "rr")
int quantumCycles; //for round robin, how many ticks before swapping (1-2^32 inclusive)
int batchProcessFreq; //1 process every x cycles (1-2^32 inclusive)
int minIns;
int maxIns;
int delaysPerExec; //1 instruction every x cycles (0 - 2^32 inclusive) if 0, it executes every cycle




void readConfig(){
    std::ifstream file("config.txt");
    std::string line;

    while(std::getline(file, line)){
        std::istringstream iss(line);
        std::string key;
        iss >> key;

        if (key == "num-cpu") {
            iss >> numCPU;
        } 
        else if (key == "scheduler") {
            iss >> std::quoted(schedulerType);
        } 
        else if (key == "quantum-cycles") {
            iss >> quantumCycles;
        } 
        else if (key == "batch-process-freq") {
            iss >> batchProcessFreq;
        } 
        else if (key == "min-ins") {
            iss >> minIns;
        } 
        else if (key == "max-ins") {
            iss >> maxIns;
        } 
        else if (key == "delays-per-exec") {
            iss >> delaysPerExec;
        }

    }
}

// Enhanced Process class
class Process {
private:
    std::string name;
    int id;
    int totalInstructions;
    int remainingInstructions;
    int currentInstruction;
    std::string timeCreated;
    int assignedCore;
    std::unique_ptr<std::ofstream> logFile;
    std::mutex processExecutionMutex; // Mutex to protect process execution
    std::vector<std::string> logs; // Store execution logs
    
public:
    // Constructor
    Process(const std::string& processName, int numInstructions = 100) 
        : name(processName), totalInstructions(numInstructions), 
          remainingInstructions(numInstructions), currentInstruction(0), assignedCore(-1) {
        
        id = processIdCounter++;
        
        // Set creation time
        std::time_t timestamp;
        std::time(&timestamp);
        char buffer[30];
        std::tm* timeinfo = std::localtime(&timestamp);
        std::strftime(buffer, sizeof(buffer), "%m/%d/%Y %I:%M:%S%p", timeinfo);
        timeCreated = buffer;

        
    }
    
    // Delete copy constructor and assignment operator to prevent copying
    Process(const Process&) = delete;
    Process& operator=(const Process&) = delete;
    
    // Move constructor
    Process(Process&& other) noexcept
        : name(std::move(other.name)), id(other.id), totalInstructions(other.totalInstructions),
          remainingInstructions(other.remainingInstructions), currentInstruction(other.currentInstruction),
          timeCreated(std::move(other.timeCreated)), assignedCore(other.assignedCore),
          logFile(std::move(other.logFile)) {
    }
    
    // Move assignment operator
    Process& operator=(Process&& other) noexcept {
        if (this != &other) {
            name = std::move(other.name);
            id = other.id;
            totalInstructions = other.totalInstructions;
            remainingInstructions = other.remainingInstructions;
            currentInstruction = other.currentInstruction;
            timeCreated = std::move(other.timeCreated);
            assignedCore = other.assignedCore;
            logFile = std::move(other.logFile);
        }
        return *this;
    }
    
    // Destructor
    ~Process() {
        if (logFile && logFile->is_open()) {
            logFile->close();
        }
    }
    
    // Execute one instruction of the process
    void executeInstruction(int coreId) {
        std::lock_guard<std::mutex> lock(processExecutionMutex);
        
        if (remainingInstructions > 0) {
            currentInstruction++;
            remainingInstructions--;
            assignedCore = coreId;
            
            // Log the print command
            std::time_t timestamp; // Get current time
            std::time(&timestamp); // Get current time in seconds since epoch
            char buffer[30]; // Format time as MM/DD/YYYY HH:MM:SS AM/PM
            std::tm* timeinfo = std::localtime(&timestamp); // Convert to local time
            std::strftime(buffer, sizeof(buffer), "%m/%d/%Y %I:%M:%S%p", timeinfo); // Format time

            //TODO: Remove writing to .txt file, instead execute the instruction on console [PRINT ONLY]
           
           // Log the print command

            std::ostringstream oss; 
            oss << "(" << buffer << ") Core:" << coreId << " \"Hello world from " << name << "!\"";
            logs.push_back(oss.str())

            
            // Simulate instruction execution time (reduced for faster testing)
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
    
    // Check if process can execute (thread-safe)
    bool canExecute() const {
        return remainingInstructions > 0;
    }
    
    // Getters
    int getRemainingInstructions() const { return remainingInstructions; }
    int getCurrentInstruction() const { return currentInstruction; }
    int getTotalInstructions() const { return totalInstructions; }
    bool hasFinished() const { return remainingInstructions == 0; }
    std::string getName() const { return name; }
    int getId() const { return id; }
    std::string getTimeCreated() const { return timeCreated; }
    int getAssignedCore() const { return assignedCore; }
    
    void setAssignedCore(int core) { assignedCore = core; }
    const std::vector<std::string>& getLogs() const { return logs; }
};

// Enhanced Screen class
class Screen {
public:
    std::string screenName;
    int curInstruction;
    int totalInstruction;
    std::string timeCreated;
    bool isDetached;
    
    Screen() : isDetached(false) {}
};

std::vector<Screen> screenList;

// Utility functions
void setColor(int color) {
    switch(color) {
        case 7:  std::cout << "\033[37m"; break;  // White
        case 10: std::cout << "\033[32m"; break;  // Green
        case 14: std::cout << "\033[33m"; break;  // Yellow
        default: std::cout << "\033[37m"; break;  // Default to white
    }
}

void printASCII(std::string fileName) {
    std::string line = "";
    std::ifstream inFile;
    inFile.open(fileName);
    if(inFile.is_open()) {
        while(std::getline(inFile, line)) {
            std::cout << line << std::endl;
        }
    } else {
        std::cout << "File failed to load. " << std::endl;
    }
    inFile.close();
}

// ASCII CSOPESY
void intro() {
    std::string fileName = "ascii.txt";
    printASCII(fileName);
    setColor(10);
    std::cout << "Hello, Welcome to CSOPESY commandline!" << std::endl;
    setColor(14);
    std::cout << "Type 'exit' to quit, 'clear' to clear the screen." << std::endl;
}


// FCFS Scheduler class
class FCFSScheduler {
private:
    int numCores = numCPU; // Number of cores
    std::vector<std::thread> coreThreads;
    std::queue<Process*> processQueue;
    std::mutex queueMutex;
    std::condition_variable queueCV;
    std::atomic<bool> running{false};

public:
    FCFSScheduler(int cores) : numCores(cores)/*, currentProcess(nullptr)*/ {}
    
    ~FCFSScheduler() {
        stop();
    }
    
    void addProcess(Process* process) {
        std::lock_guard<std::mutex> lock(queueMutex);
        processQueue.push(process);
        queueCV.notify_all();
    }
    
    void start() {
        running = true;
        schedulerRunning = true;
        
        // Create threads for each core
        for (int i = 0; i < numCores; i++) {
            coreThreads.emplace_back([this, i]() {
                this->coreWorker(i);
            });
        }
        
        std::cout << "Scheduler started with " << numCores << " cores." << std::endl << std::endl;
    }
    
    void stop() {

        running = false;
        schedulerRunning = false;
        queueCV.notify_all();
        
        for (auto& thread : coreThreads) {
            if (thread.joinable()) {
                thread.join();
            }
        }
        coreThreads.clear();
        
        std::cout << "Scheduler stopped." << std::endl;
    }

    void coreWorker(int coreId) {
        while (running) {
            Process* processToExecute = nullptr;

            // Get a process from the queue
            {
                std::unique_lock<std::mutex> lock(queueMutex);
                queueCV.wait(lock, [this]() { return !processQueue.empty() || !running; });
            
                if (!running) return;

                processToExecute = processQueue.front();
                processQueue.pop();

                {
                    std::lock_guard<std::mutex> pLock(processMutex);
                    runningProcesses.push_back(processToExecute);
                }
            }

        // Assign process to this core and execute until it's done
            processToExecute->setAssignedCore(coreId);
            while (processToExecute->canExecute() && running) {
                processToExecute->executeInstruction(coreId);
            }

            // Move to finished
            {
                std::lock_guard<std::mutex> pLock(processMutex);
                runningProcesses.erase(
                    std::remove_if(runningProcesses.begin(), runningProcesses.end(),
                        [processToExecute](const Process* p) { return p->getId() == processToExecute->getId(); }),
                    runningProcesses.end());

                finishedProcesses.push_back(processToExecute);
            }
        }
    }   
    
    bool isRunning() const { return running; }
};

//TODO: Round Robin
class RoundRobinScheduler{
// maybe copy paste FCFS then just modify to include quantum cycles
};

void clearScreen() {
    #ifdef _WIN32
        system("cls");
    #else
        system("clear");
    #endif
}



void initialize() {
    readConfig();
    std::cout << "-------------------------------------------------------------------------" << std::endl;
    std::cout << "Number of Cores: " << numCPU << std::endl;
    std::cout << "Scheduler Type: " << schedulerType << std::endl;
    std::cout << "Quantum Cycles: " << quantumCycles << std::endl;
    std::cout << "Batch Process Frequency: " << batchProcessFreq << std::endl;
    std::cout << "Minimum Instructions: " << minIns << std::endl;
    std::cout << "Maximum Instructions: " << maxIns << std::endl;
    std::cout << "Delay per Execution: " << delaysPerExec << std::endl;
    std::cout << "-------------------------------------------------------------------------" << std::endl;
    std::cout << "System Initialized. You may now create screens and perform other actions.\n\n";
    
}

Screen curScreen;

void createScreen(std::string &screenName) {
    std::time_t timestamp;
    std::time(&timestamp);

    Screen newScreen;
    newScreen.screenName = screenName;
    newScreen.totalInstruction = 100; // TODO: random number between min-ins and max-ins (inclusive)
    newScreen.curInstruction = 0;
    
    char buffer[30];
    std::tm* timeinfo = std::localtime(&timestamp); 
    std::strftime(buffer, sizeof(buffer), "%m/%d/%Y %I:%M:%S%p", timeinfo);
    newScreen.timeCreated = buffer;
    newScreen.isDetached = false;

    screenList.emplace_back(newScreen);
    curScreen = newScreen;
    // Create corresponding process using unique_ptr
    auto newProcess = std::make_unique<Process>(screenName, 100);
    allProcesses.push_back(std::move(newProcess));
}



void screenLS() {
    std::lock_guard<std::mutex> lock(processMutex);
    
    std::cout << "================\n";
    std::cout << "Running processes:\n";
    
    for (const auto& process : runningProcesses) {
        std::cout << process->getName() << " (" << process->getTimeCreated() << ")  "
                  << "Core: " << process->getAssignedCore() << "  "
                  << process->getCurrentInstruction() << "  /  " << process->getTotalInstructions() << std::endl;
    }
    std::cout << "\n\n------------------\n\n";
    
    std::cout << "\nFinished processes:\n";
    for (const auto& process : finishedProcesses) {
        std::cout << process->getName() << "  (" << process->getTimeCreated() << ")  "
                  << "Finished " << process->getTotalInstructions() << "  /  " << process->getTotalInstructions() << std::endl;
    }
    
    std::cout << "================\n\n";
}



void screen(std::string &screenCommand) {
    std::istringstream iss(screenCommand);
    std::string screen, option, screenName;
    iss >> screen >> option >> screenName;

    if(option == "-s" && !screenName.empty()) {
        createScreen(screenName);
        clearScreen();
        std::cout << "Screen created: " << curScreen.screenName << std::endl;
        std::cout << "Instructions: " << curScreen.curInstruction << " out of " << curScreen.totalInstruction << std::endl;
        std::cout << "Time Created: " << curScreen.timeCreated << std::endl << std::endl;
        
        // Add process to scheduler if it's running
        if (scheduler && scheduler->isRunning()) {
            scheduler->addProcess(allProcesses.back().get());
        }
    }
    else if(option == "-r" && !screenName.empty()) {
        bool found = false;
        for(auto& scr : screenList) {
            if(scr.screenName == screenName) {
                clearScreen();

                curScreen = scr;
                std::cout << "Screen: " << curScreen.screenName << std::endl;
                // Find corresponding process
                auto it = std::find_if(allProcesses.begin(), allProcesses.end(),
                    [&screenName](const std::unique_ptr<Process>& p) { return p->getName() == screenName; });
                
                if (it != allProcesses.end()) {
                    std::cout << "Running instruction: " << (*it)->getCurrentInstruction() 
                              << " out of " << (*it)->getTotalInstructions() << std::endl;
                } else {
                    std::cout << "Running instruction: " << curScreen.curInstruction 
                              << " out of " << curScreen.totalInstruction << std::endl;
                }
                
                std::cout << "Time Created: " << curScreen.timeCreated << std::endl << std::endl;
                scr.isDetached = false;
                found = true;
                break;
            }
        }
        if(!found) {
            std::cout << "Screen \"" << screenName << "\" not found." << std::endl << std::endl;
        }
    }
    else if(option == "-d" && !screenName.empty()) {
        bool found = false;
        for(auto& scr : screenList) {
            if(scr.screenName == screenName) {
                scr.isDetached = true;
                std::cout << "Screen \"" << screenName << "\" detached." << std::endl << std::endl;
                found = true;
                break;
            }
        }
        if(!found) {
            std::cout << "Screen \"" << screenName << "\" not found." << std::endl << std::endl;
        }
    }
    else if(option == "-ls") {
        screenLS();
        return;
    }

    if(option != "-ls") {
        std::string screenInput;
        while(screenInput != "exit") {
            std::cout << "Enter a command, or type exit to return to the main menu: ";
            std::getline(std::cin, screenInput);
            if(screenInput == "process-smi") {
                for (const auto& process : allProcesses) {
                std::cout << "\nProcess name: " << process->getName() << std::endl;
                std::cout << "ID: " << process->getId() << std::endl;
                std::cout << "Logs:" << std::endl;
                for (const auto& logEntry : process->getLogs()) {
                    std::cout << logEntry << std::endl;
                }
                std::cout << "\nCurrent instruction line: " << process->getCurrentInstruction() << std::endl;
                std::cout << "Lines of code: " << process->getTotalInstructions() << "\n" << std::endl;
                }   
            }
            else if (screenInput == "exit"){
                clearScreen();
                intro();
            }
            else{
                std::cout << "Invalid command. You can only input 'process-smi' or 'exit'. " << std::endl << std::endl;
            }
        }
    }

    

}

void schedulerStart() {
    std::cout << "Scheduler started. Creating processes indefinitely...\n"<< "Type 'scheduler-stop' to halt process generation.\n\n";
    for (int i = 0; i < 5; ++i) { // TODO: Change to infinite loop, interruptible by 'scheduler-stop'
        std::string baseName = "autogen-process-";
        std::string uniqueScreenName = baseName + std::to_string(i + 1); // Appends 1, 2, 3, etc.
        createScreen(uniqueScreenName);
    }
    std::cout << "Process generation ceased.\n\n";
} 

void schedulerStop() {
    // if (scheduler && scheduler->isRunning()) {
    //     scheduler->stop();
    // } else {
    //     std::cout << "Scheduler is not running." << std::endl << std::endl;
    // }
}

void reportUtil() {
    //TODO: screen -ls but put into a .txt file
    
    std::ofstream outFile("csopesy-log.txt");
    if (!outFile.is_open()) {
        std::cerr << "Failed to open file for writing.\n";
        return;
    }

    outFile << "================\n";
    outFile << "Running processes:\n";

    for (const auto& process : runningProcesses) {
        outFile << process->getName() << " (" << process->getTimeCreated() << ")  "
                << "Core: " << process->getAssignedCore() << "  "
                << process->getCurrentInstruction() << "  /  " << process->getTotalInstructions() << "\n";
    }

    outFile << "\n\n------------------\n\n";

    outFile << "\nFinished processes:\n";
    for (const auto& process : finishedProcesses) {
        outFile << process->getName() << "  (" << process->getTimeCreated() << ")  "
                << "Finished " << process->getTotalInstructions() << "  /  " << process->getTotalInstructions() << "\n";
    }

    outFile << "================\n\n";

    outFile.close();
    std::cout << "Report generated at opesy-log.txt!\n\n";


}


void menu() {
    std::string input;
    bool initialized = false;
    intro();
    while (true) {
        
        setColor(7);
        std::cout << "Enter a command: ";
        std::string command;
        std::getline(std::cin >> std::ws, command);

        if(command == "exit") {
            if (scheduler) {
                scheduler->stop();
                delete scheduler;
            }
            std::cout << "Thank you for using the program";
            exit(0);
        }
        else if (command == "clear") {
            clearScreen();
            intro();
        }
        else if(command == "initialize") {
            initialize();
            initialized = true;
        }
        else if (!initialized) {
            std::cout << "Command is not recognized. Please initialize the system first by using the 'initialize' command.\n\n";
        }
        else if(command.rfind("screen",0) == 0) {
            screen(command);
        }
        else if(command == "scheduler-start") {
            schedulerStart();
        }
        else if(command == "scheduler-stop") {
            schedulerStop();
        }
        else if(command == "report-util") {
            reportUtil();
        }
        else {
            std::cout << "Unknown command. Please try again.\n\n";
        }
    }
}

int main() {
    std::srand(static_cast<unsigned int>(std::time(nullptr)));
    menu();
    return 0;
}