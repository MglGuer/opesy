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
#include <list>

// Forward declarations
class Process;
class Scheduler;
class FCFSScheduler;
class RoundRobinScheduler;


// Global variables
//std::vector<std::unique_ptr<Process>> allProcesses;
std::list<std::unique_ptr<Process>> allProcesses;
std::vector<Process*> runningProcesses;
std::vector<Process*> finishedProcesses;
Scheduler* scheduler = nullptr;
std::mutex processMutex;
std::atomic<bool> schedulerRunning{false};
std::atomic<int> processIdCounter{1};
std::atomic<long long> global_simulated_cycles{0}; // Global variable to track simulated cycles
std::atomic<bool> processCreationRunning{false}; 
std::atomic<int> autoProcessCounter{0}; //for tracking auto-generated screen names

//FOR CONFIG.txt
int numCPU; //number of cores (between 1-128 inclusive)
std::string schedulerType; //scheduler type("fcfs" or "rr")
int quantumCycles; //for round robin, how many ticks before swapping (1-2^32 inclusive)
int batchProcessFreq; //1 process every x cycles (1-2^32 inclusive)
int minIns;
int maxIns;
int delaysPerExec; //1 instruction every x cycles (0 - 2^32 inclusive) if 0, it executes every cycle

//Instruction Types
enum class InstructionType {
    PRINT,
    ADD,
    DECLARE,
    SUBTRACT,
    SLEEP,
    FOR
};

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

// Process class
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
    std::atomic<int> cyclesSinceLastExec{0}; // For delays-per-exec implementation
    
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
    bool executeInstruction(int coreId) {
        std::lock_guard<std::mutex> lock(processExecutionMutex);
        
        // Implement delays-per-exec
        if (delaysPerExec > 0) {
            cyclesSinceLastExec++;
            if (cyclesSinceLastExec <= delaysPerExec) {
                return true; // Skip execution but count as a cycle
            }
            cyclesSinceLastExec = 0;
        }
        
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
            logs.push_back(oss.str());

            

            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            
            return true;
        }
        return false;
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

void cpuCycleLoop() {
    while (schedulerRunning) {
        global_simulated_cycles++;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        
        if (global_simulated_cycles % 100 == 0) {
            std::this_thread::yield();
        }
    }
}

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
    //printASCII(fileName); TODO: uncomment when done debugging.
    setColor(10);
    std::cout << "Hello, Welcome to CSOPESY commandline!" << std::endl;
    setColor(14);
    std::cout << "Type 'exit' to quit, 'clear' to clear the screen." << std::endl;

    setColor(7);
    std::cout << "\nDevelopers:\n";
    std::cout << "Dimaculangan, Renzel\n"
              << "Guerrero, Miguel\n"
              << "Valdez, Kimi\n"
              << "Velasquez, Almira Zabrina Alyson\n\n";
}

// Abstract Scheduler base class
class Scheduler {
public:
    virtual ~Scheduler() = default;
    virtual void addProcess(Process* process) = 0;
    virtual void start() = 0;
    virtual void stop() = 0;
    virtual bool isRunning() const = 0;
};

// FCFS Scheduler class
class FCFSScheduler : public Scheduler {
private:
    int numCores; //Number of cores
    std::vector<std::thread> coreThreads;
    std::queue<Process*> processQueue;
    std::mutex queueMutex;
    std::condition_variable queueCV;
    std::atomic<bool> running{false};

public:
    FCFSScheduler(int cores) : numCores(cores) {}
    
    ~FCFSScheduler() {
        stop();
    }
    
    void addProcess(Process* process) override {
        std::lock_guard<std::mutex> lock(queueMutex);
        processQueue.push(process);
        queueCV.notify_all();
    }
    
    void start() override {
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
    
    void stop() override {

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
    
    bool isRunning() const override { return running; }
};

//TODO: Round Robin - FIXED VERSION
class RoundRobinScheduler : public Scheduler {
private:
    int numCores;
    int quantum;
    std::vector<std::thread> coreThreads;
    std::queue<Process*> processQueue;
    std::mutex queueMutex;
    std::condition_variable queueCV;
    std::atomic<bool> running{false};
    std::set<int> activeProcessIds; // Track which processes are being executed

public:
    RoundRobinScheduler(int cores, int quantumCycles) : numCores(cores), quantum(quantumCycles) {}

    ~RoundRobinScheduler() {
        stop();
    }

    void addProcess(Process* process) override {
        {
            std::lock_guard<std::mutex> lock(queueMutex);
            processQueue.push(process);
        }
        queueCV.notify_one();
    }

    void start() override {
        running = true;
        schedulerRunning = true;

        for (int i = 0; i < numCores; ++i) {
            coreThreads.emplace_back([this, i]() {
                this->coreWorker(i);
            });
        }
        std::cout << "Round Robin Scheduler started with " << numCores << " cores and quantum of " << quantum << " cycles." << std::endl << std::endl;
    }

    void stop() override {
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

            {
                std::unique_lock<std::mutex> lock(queueMutex);
                queueCV.wait(lock, [this] { return !processQueue.empty() || !running; });

                if (!running) return;

                processToExecute = processQueue.front();
                processQueue.pop();
                
                // Check if this process is already being executed by another core
                if (activeProcessIds.find(processToExecute->getId()) != activeProcessIds.end()) {
                    // Re-queue and try again
                    processQueue.push(processToExecute);
                    continue;
                }
                
                activeProcessIds.insert(processToExecute->getId());
            }

            {
                std::lock_guard<std::mutex> pLock(processMutex);
                if (std::find(runningProcesses.begin(), runningProcesses.end(), processToExecute) == runningProcesses.end()) {
                    runningProcesses.push_back(processToExecute);
                }
            }

            processToExecute->setAssignedCore(coreId);

            // Execute for quantum cycles
            int executedCycles = 0;
            while (executedCycles < quantum && processToExecute->canExecute() && running) {
                processToExecute->executeInstruction(coreId);
                executedCycles++;
                
                // Small yield periodically to prevent CPU hogging
                if (executedCycles % 10 == 0) {
                    std::this_thread::yield();
                }
            }

            {
                std::lock_guard<std::mutex> lock(queueMutex);
                activeProcessIds.erase(processToExecute->getId());
            }

            if (processToExecute->hasFinished()) {
                std::lock_guard<std::mutex> pLock(processMutex);
                runningProcesses.erase(std::remove(runningProcesses.begin(), runningProcesses.end(), processToExecute), runningProcesses.end());
                finishedProcesses.push_back(processToExecute);
            } else if (running) {
                addProcess(processToExecute);
            }
        }
    }

    bool isRunning() const override { return running; }
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
    int instructionCount = minIns + (std::rand() % (maxIns - minIns + 1));
    newScreen.totalInstruction = instructionCount;
    newScreen.curInstruction = 0;
    
    char buffer[30];
    std::tm* timeinfo = std::localtime(&timestamp); 
    std::strftime(buffer, sizeof(buffer), "%m/%d/%Y %I:%M:%S%p", timeinfo);
    newScreen.timeCreated = buffer;
    newScreen.isDetached = false;

    screenList.emplace_back(newScreen);
    curScreen = newScreen;
    // Create corresponding process using unique_ptr
    auto newProcess = std::make_unique<Process>(screenName, instructionCount);
    allProcesses.push_back(std::move(newProcess));
}



void screenLS() {
    std::lock_guard<std::mutex> lock(processMutex);
    
    // Calculate CPU utilization
    std::set<int> usedCores;
    for (const auto& process : runningProcesses) {
        if (process->getAssignedCore() != -1) {
            usedCores.insert(process->getAssignedCore());
        }
    }
    
    int coresUsed = usedCores.size();
    int coresAvailable = numCPU - coresUsed;
    double cpuUtilization = (numCPU > 0) ? (static_cast<double>(coresUsed) / numCPU * 100.0) : 0.0;
    
    std::cout << "CPU utilization: " << std::fixed << std::setprecision(2) << cpuUtilization << "%\n";
    std::cout << "Cores used: " << coresUsed << "\n";
    std::cout << "Cores available: " << coresAvailable << "\n";
    
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

    if(option == "-s") {
        // If no screen name provided, create multiple processes
        if(screenName.empty()) {
            // Create 3 processes: proc-01, proc-02, proc-03
            for(int i = 1; i <= 3; i++) {
                std::string procName = "proc-";
                if(i < 10) procName += "0";
                procName += std::to_string(i);
                createScreen(procName);
                
                // Add process to scheduler if it's running
                if (scheduler && scheduler->isRunning()) {
                    scheduler->addProcess(allProcesses.back().get());
                }
            }
            std::cout << "3 processes created successfully: proc-01, proc-02, proc-03" << std::endl << std::endl;
            return;
        } else {
            // Create single process with given name
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
    }
    else if(option == "-r" && !screenName.empty()) {
        bool found = false;
        
        // First check if process is finished
        auto finishedIt = std::find_if(finishedProcesses.begin(), finishedProcesses.end(),
            [&screenName](const Process* p) { return p->getName() == screenName; });
        
        if (finishedIt != finishedProcesses.end()) {
            std::cout << "Process \"" << screenName << "\" has already finished execution and cannot be accessed." << std::endl << std::endl;
            return;
        }
        
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

    if(option == "-r" && !screenName.empty()) {
        std::string screenInput;
        while(screenInput != "exit") {
            std::cout << "Enter a command, or type exit to return to the main menu: ";
            std::getline(std::cin, screenInput);
            if(screenInput == "process-smi") {
                // Find the specific process for this screen
                auto it = std::find_if(allProcesses.begin(), allProcesses.end(),
                    [&screenName](const std::unique_ptr<Process>& p) { return p->getName() == screenName; });
                
                if (it != allProcesses.end()) {
                    Process* process = it->get();
                    std::cout << "\nProcess name: " << process->getName();
                    
                    if (process->hasFinished()) {
                        std::cout << " Finished!" << std::endl;
                    } else {
                        std::cout << std::endl;
                    }
                    
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

void processCreationLoop() {
    static int i = 0;
    long long lastCycle = 0;
    
    // Wait for scheduler to start up
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    while (processCreationRunning) {
        // Only create processes if scheduler is running AND we have available cores
        if (scheduler && scheduler->isRunning()) {
            // Check if we have available cores before creating new processes
            int coresInUse = 0;
            {
                std::lock_guard<std::mutex> lock(processMutex);
                std::set<int> usedCores;
                for (const auto& process : runningProcesses) {
                    if (process->getAssignedCore() != -1) {
                        usedCores.insert(process->getAssignedCore());
                    }
                }
                coresInUse = usedCores.size();
            }
            
            // Only create new process if we have available cores AND enough cycles have passed
            if (coresInUse < numCPU && (global_simulated_cycles - lastCycle >= batchProcessFreq)) {
                std::string processName = "process_";
                if (i < 10) {
                    processName += "0";
                }
                processName += std::to_string(i++);
                
                createScreen(processName);
                scheduler->addProcess(allProcesses.back().get());
                
                lastCycle = global_simulated_cycles;
            }
        }
        
        // Longer sleep to prevent overwhelming the system
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void schedulerStart() {
    if (scheduler == nullptr) {
        if (schedulerType == "fcfs") {
            scheduler = new FCFSScheduler(numCPU);
        } else if (schedulerType == "rr") {
            scheduler = new RoundRobinScheduler(numCPU, quantumCycles);
        } else {
            std::cout << "Error: Unknown scheduler type '" << schedulerType << "' in config.txt. Aborting." << std::endl;
            return;
        }
    }

    if (!scheduler->isRunning()) {
        std::cout << "Creating processes. Enter \"scheduler-stop\" to cease." << std::endl;
        processCreationRunning = true;
        schedulerRunning = true;
        std::thread(cpuCycleLoop).detach();         // Start CPU cycle simulation
        std::thread(processCreationLoop).detach();  // Start process creation in background
        scheduler->start();
    } else {
        std::cout << "Scheduler is already running." << std::endl << std::endl;
    }
}

void schedulerStop() {
    std::cout << "Stopping scheduler and process creation..." << std::endl;
    
    // Stop new process creation first
    processCreationRunning = false;
    
    // Give a moment for process creation to stop
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    if (scheduler != nullptr && scheduler->isRunning()) {
        scheduler->stop();
        
        // Wait a bit for all processes to finish naturally
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        
        {
            std::lock_guard<std::mutex> pLock(processMutex);
            
            // Move any remaining running processes to finished
            for (Process* proc : runningProcesses) {
                if (proc->hasFinished()) {
                    finishedProcesses.push_back(proc);
                } else {
                    // Mark incomplete processes as finished for clean shutdown
                    finishedProcesses.push_back(proc);
                }
            }
            
            // Clear running processes to show 0% utilization
            runningProcesses.clear();
        }
        
        std::cout << "Scheduler stopped." << std::endl;
        std::cout << "Total screens/processes created: " << screenList.size() << std::endl;
    } else {
        std::cout << "Scheduler is not running." << std::endl;
    }
    
    // Stop CPU cycle simulation
    schedulerRunning = false;
}

void reportUtil() {
    //TODO: screen -ls but put into a .txt file
    
    std::ofstream outFile("csopesy-log.txt");
    if (!outFile.is_open()) {
        std::cerr << "Failed to open file for writing.\n";
        return;
    }

    std::lock_guard<std::mutex> lock(processMutex);

    std::set<int> usedCores;
    for (const auto& process : runningProcesses) {
        if (process->getAssignedCore() != -1) {
            usedCores.insert(process->getAssignedCore());
        }
    }
    
    int coresUsed = usedCores.size();
    int coresAvailable = numCPU - coresUsed;
    double cpuUtilization = (numCPU > 0) ? (static_cast<double>(coresUsed) / numCPU * 100.0) : 0.0;
    
    outFile << "CPU utilization: " << std::fixed << std::setprecision(2) << cpuUtilization << "%\n";
    outFile << "Cores used: " << coresUsed << "\n";
    outFile << "Cores available: " << coresAvailable << "\n";

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
        std::cout << "root:\\> ";
        std::string command;
        std::getline(std::cin >> std::ws, command);

        if(command == "exit") {
            if (scheduler != nullptr) {
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