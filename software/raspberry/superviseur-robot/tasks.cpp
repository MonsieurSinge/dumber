/*
 * Copyright (C) 2018 dimercur
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "tasks.h"
#include "lib/messages.h"
#include "lib/comrobot.h"
#include <stdexcept>

// Déclaration des priorités des taches
#define PRIORITY_TSERVER 30
#define PRIORITY_TOPENCOMROBOT 20
#define PRIORITY_TMOVE 20
#define PRIORITY_TSENDTOMON 22
#define PRIORITY_TRECEIVEFROMMON 25
#define PRIORITY_TSTARTROBOT 20
#define PRIORITY_TCAMERA 21
#define PRIORITY_TBATTERY 27
#define PRIORITY_TWATCHDOG 28

#define RETRY_ERR_ROBOT 3

/*
 * Some remarks:
 * 1- This program is mostly a template. It shows you how to create tasks, semaphore
 *   message queues, mutex ... and how to use them
 * 
 * 2- semDumber is, as name say, useless. Its goal is only to show you how to use semaphore
 * 
 * 3- Data flow is probably not optimal
 * 
 * 4- Take into account that ComRobot::Write will block your task when serial buffer is full,
 *   time for internal buffer to flush
 * 
 * 5- Same behavior existe for ComMonitor::Write !
 * 
 * 6- When you want to write something in terminal, use cout and terminate with endl and flush
 * 
 * 7- Good luck !
 */

/**
 * @brief Initialisation des structures de l'application (tâches, mutex, 
 * semaphore, etc.)
 */
void Tasks::Init() {
    int status;
    int err;

    /**************************************************************************************/
    /* 	Mutex creation                                                                    */
    /**************************************************************************************/
    if ((err = rt_mutex_create(&mutex_monitor, nullptr))) {
        cerr << "Error mutex create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if ((err = rt_mutex_create(&mutex_robot, nullptr))) {
        cerr << "Error mutex create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if ((err = rt_mutex_create(&mutex_robotStarted, nullptr))) {
        cerr << "Error mutex create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if ((err = rt_mutex_create(&mutex_robotConnected, nullptr))) {
        cerr << "Error mutex create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if ((err = rt_mutex_create(&mutex_monitorConnected, nullptr))) {
        cerr << "Error mutex create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if ((err = rt_mutex_create(&mutex_move, nullptr))) {
        cerr << "Error mutex create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    cout << "Mutexes created successfully" << endl << flush;

    /**************************************************************************************/
    /* 	Semaphors creation       							  */
    /**************************************************************************************/
    if ((err = rt_sem_create(&sem_barrier, nullptr, 0, S_FIFO))) {
        cerr << "Error semaphore create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if ((err = rt_sem_create(&sem_openComRobot, nullptr, 0, S_FIFO))) {
        cerr << "Error semaphore create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if ((err = rt_sem_create(&sem_serverOk, nullptr, 0, S_FIFO))) {
        cerr << "Error semaphore create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if ((err = rt_sem_create(&sem_startRobot, nullptr, 0, S_FIFO))) {
        cerr << "Error semaphore create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if ((err = rt_sem_create(&sem_monitor, nullptr, 0, S_FIFO))) {
        cerr << "Error semaphore create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if ((err = rt_sem_create(&sem_startWithWD, nullptr, 0, S_FIFO))) {
        cerr << "Error semaphore create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    cout << "Semaphores created successfully" << endl << flush;

    /**************************************************************************************/
    /* Tasks creation                                                                     */
    /**************************************************************************************/
    if ((err = rt_task_create(&th_server, "th_server", 0, PRIORITY_TSERVER, 0))) {
        cerr << "Error task create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if ((err = rt_task_create(&th_sendToMon, "th_sendToMon", 0, PRIORITY_TSENDTOMON, 0))) {
        cerr << "Error task create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if ((err = rt_task_create(&th_receiveFromMon, "th_receiveFromMon", 0, PRIORITY_TRECEIVEFROMMON, 0))) {
        cerr << "Error task create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if ((err = rt_task_create(&th_openComRobot, "th_openComRobot", 0, PRIORITY_TOPENCOMROBOT, 0))) {
        cerr << "Error task create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if ((err = rt_task_create(&th_startRobot, "th_startRobot", 0, PRIORITY_TSTARTROBOT, 0))) {
        cerr << "Error task create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if ((err = rt_task_create(&th_move, "th_move", 0, PRIORITY_TMOVE, 0))) {
        cerr << "Error task create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if ((err = rt_task_create(&th_battery, "th_battery", 0, PRIORITY_TBATTERY, 0))) {
        cerr << "Error task create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if ((err = rt_task_create(&th_watchdog, "th_watchdog", 0, PRIORITY_TWATCHDOG, 0))) {
        cerr << "Error task create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    cout << "Tasks created successfully" << endl << flush;

    /**************************************************************************************/
    /* Message queues creation                                                            */
    /**************************************************************************************/
    if ((err = rt_queue_create(&q_messageToMon, "q_messageToMon", sizeof (Message*)*50, Q_UNLIMITED, Q_FIFO)) < 0) {
        cerr << "Error msg queue create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    cout << "Queues created successfully" << endl << flush;

}

/**
 * @brief Démarrage des tâches
 */
void Tasks::Run() {
    rt_task_set_priority(nullptr, T_LOPRIO);
    int err;

    if (err = rt_task_start(&th_server, (void(*)(void*)) & Tasks::ServerTask, this)) {
        cerr << "Error task start: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_task_start(&th_sendToMon, (void(*)(void*)) & Tasks::SendToMonTask, this)) {
        cerr << "Error task start: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_task_start(&th_receiveFromMon, (void(*)(void*)) & Tasks::ReceiveFromMonTask, this)) {
        cerr << "Error task start: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_task_start(&th_openComRobot, (void(*)(void*)) & Tasks::OpenComRobot, this)) {
        cerr << "Error task start: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_task_start(&th_startRobot, (void(*)(void*)) & Tasks::StartRobotTask, this)) {
        cerr << "Error task start: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_task_start(&th_move, (void(*)(void*)) & Tasks::MoveTask, this)) {
        cerr << "Error task start: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_task_start(&th_battery, (void(*)(void*)) & Tasks::BatteryTask, this)) {
        cerr << "Error task start: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_task_start(&th_watchdog, (void(*)(void*)) & Tasks::WatchDogTask, this)) {
        cerr << "Error task start: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }

    cout << "Tasks launched" << endl << flush;
}

/**
 * @brief Arrêt des tâches
 */
void Tasks::Stop() {
    rt_mutex_acquire(&mutex_monitorConnected, TM_INFINITE);
    monitorConnected = false;
    rt_mutex_release(&mutex_monitorConnected);
    monitor.Close();
    this->CloseRobot();
}

void Tasks::CloseRobot() {
    rt_mutex_acquire(&mutex_robotStarted, TM_INFINITE);
    robotStarted = 0;
    rt_mutex_release(&mutex_robotStarted);
    rt_mutex_acquire(&mutex_robotConnected, TM_INFINITE);
    robotConnected = false;
    rt_mutex_release(&mutex_robotConnected);
    robot.Close();
    this->robot_err_counter = 0;
}

/**
 */
void Tasks::Join() {
    cout << "Tasks synchronized" << endl << flush;
    rt_sem_broadcast(&sem_barrier);
    pause();
}

/**
 * @brief Thread handling server communication with the monitor.
 */
void Tasks::ServerTask(void *arg) {
    int status;

        cout << "Start " << __PRETTY_FUNCTION__ << endl << flush;
        // Synchronization barrier (waiting that all tasks are started)
        rt_sem_p(&sem_barrier, TM_INFINITE);

    while(true) {
        /**************************************************************************************/
        /* The task server starts here                                                        */
        /**************************************************************************************/
        rt_mutex_acquire(&mutex_monitor, TM_INFINITE);
        status = monitor.Open(SERVER_PORT);
        rt_mutex_release(&mutex_monitor);

        cout << "Open server on port " << (SERVER_PORT) << " (" << status << ")" << endl;

        if (status < 0)
            throw std::runtime_error{
                    "Unable to start server on port " + std::to_string(SERVER_PORT)
            };

        monitor.AcceptClient(); // Wait the monitor client
        rt_mutex_acquire(&mutex_monitorConnected, TM_INFINITE);
        monitorConnected = true;
        rt_mutex_release(&mutex_monitorConnected);
        cout << "Rock'n'Roll baby, client accepted!" << endl << flush;
        rt_sem_broadcast(&sem_serverOk);
        rt_sem_p(&sem_monitor, TM_INFINITE);
    }
}

/**
 * @brief Thread sending data to monitor.
 */
[[noreturn]] void Tasks::SendToMonTask(void* arg) {
    Message *msg;
    
    cout << "Start " << __PRETTY_FUNCTION__ << endl << flush;
    // Synchronization barrier (waiting that all tasks are starting)
    rt_sem_p(&sem_barrier, TM_INFINITE);

    /**************************************************************************************/
    /* The task sendToMon starts here                                                     */
    /**************************************************************************************/
    rt_sem_p(&sem_serverOk, TM_INFINITE);

    while (true) {
        cout << "wait msg to send" << endl << flush;
        msg = ReadInQueue(&q_messageToMon);
        cout << "Send msg to mon: " << msg->ToString() << endl << flush;
        bool mc = false;
        rt_mutex_acquire(&mutex_monitorConnected, TM_INFINITE);
        mc = monitorConnected;
        rt_mutex_release(&mutex_monitorConnected);
        if(mc) {
            rt_mutex_acquire(&mutex_monitor, TM_INFINITE);
            monitor.Write(msg); // The message is deleted with the Write
            rt_mutex_release(&mutex_monitor);
        }
    }
}

/**
 * @brief Thread receiving data from monitor.
 */
[[noreturn]] void Tasks::ReceiveFromMonTask(void *arg) {
    Message *msgRcv;
    int err;
    bool connected;
    
    cout << "Start " << __PRETTY_FUNCTION__ << endl << flush;
    // Synchronization barrier (waiting that all tasks are starting)
    rt_sem_p(&sem_barrier, TM_INFINITE);

    /**************************************************************************************/
    /* The task receiveFromMon starts here                                                */
    /**************************************************************************************/
    while(true) {
        rt_sem_p(&sem_serverOk, TM_INFINITE);
        cout << "Received message from monitor activated" << endl << flush;

        connected = true;
        while (connected) {
            msgRcv = monitor.Read();
            cout << "Rcv <= " << msgRcv->ToString() << endl << flush;

            if (msgRcv->CompareID(MESSAGE_MONITOR_LOST)) {
                cout << "Perte de communication :'( 0w0" << endl << flush;

                /* stopper le robot
                 * stopper la communication avec le robot
                 * fermer le serveur
                 * déconnecter la caméra
                 * revenir au démarrage du serveur
                 * */

                // fct 6
                // stopper le robot
                rt_mutex_acquire(&mutex_robotStarted, TM_INFINITE);
                robotStarted = 0;
                rt_mutex_release(&mutex_robotStarted);
                Message *msg = this->WriteToRobot(ComRobot::Stop());
                WriteInQueue(&q_messageToMon, msg);
                // close cam
                camera.Close();
                // stop comm robot & le serveur
                this->Stop();
                rt_sem_v(&sem_monitor);
                connected = false;
            } else if (msgRcv->CompareID(MESSAGE_ROBOT_COM_OPEN)) {
                rt_sem_v(&sem_openComRobot);
            } else if (msgRcv->CompareID(MESSAGE_ROBOT_START_WITHOUT_WD)) {
                rt_sem_v(&sem_startRobot);
            } else if (msgRcv->CompareID(MESSAGE_ROBOT_START_WITH_WD)) {
                rt_sem_v(&sem_startWithWD);
            } else if (msgRcv->CompareID(MESSAGE_ROBOT_GO_FORWARD) ||
                       msgRcv->CompareID(MESSAGE_ROBOT_GO_BACKWARD) ||
                       msgRcv->CompareID(MESSAGE_ROBOT_GO_LEFT) ||
                       msgRcv->CompareID(MESSAGE_ROBOT_GO_RIGHT) ||
                       msgRcv->CompareID(MESSAGE_ROBOT_STOP)) {

                rt_mutex_acquire(&mutex_move, TM_INFINITE);
                move = msgRcv->GetID();
                rt_mutex_release(&mutex_move);
            }
            delete (msgRcv); // mus be deleted manually, no consumer
        }
    }
}

/**
 * @brief Thread opening communication with the robot.
 */
[[noreturn]] void Tasks::OpenComRobot(void *arg) {
    int status;

    cout << "Start " << __PRETTY_FUNCTION__ << endl << flush;
    // Synchronization barrier (waiting that all tasks are starting)
    rt_sem_p(&sem_barrier, TM_INFINITE);
    
    /**************************************************************************************/
    /* The task openComRobot starts here                                                  */
    /**************************************************************************************/
    while (true) {
        rt_sem_p(&sem_openComRobot, TM_INFINITE);
        cout << "Open serial com (";
        rt_mutex_acquire(&mutex_robot, TM_INFINITE);
        status = robot.Open();
        rt_mutex_release(&mutex_robot);
        cout << status;
        cout << ")" << endl << flush;

        Message * msgSend;
        if (status < 0) {
            msgSend = new Message(MESSAGE_ANSWER_NACK);
        } else {
            msgSend = new Message(MESSAGE_ANSWER_ACK);
        }
        WriteInQueue(&q_messageToMon, msgSend); // msgSend will be deleted by sendToMon
    }
}

/**
 * @brief Thread starting the communication with the robot.
 */
[[noreturn]] void Tasks::StartRobotTask(void *arg) {
    cout << "Start " << __PRETTY_FUNCTION__ << endl << flush;
    // Synchronization barrier (waiting that all tasks are starting)
    rt_sem_p(&sem_barrier, TM_INFINITE);
    
    /**************************************************************************************/
    /* The task startRobot starts here                                                    */
    /**************************************************************************************/
    while (true) {
        Message * msgSend;
        rt_sem_p(&sem_startRobot, TM_INFINITE);
        cout << "Start robot without watchdog (";
        msgSend = this->WriteToRobot(ComRobot::StartWithoutWD());
        cout << msgSend->GetID();
        cout << ")" << endl;

        cout << "Movement answer: " << msgSend->ToString() << endl << flush;
        WriteInQueue(&q_messageToMon, msgSend);  // msgSend will be deleted by sendToMon

        if (msgSend->GetID() == MESSAGE_ANSWER_ACK) {
            rt_mutex_acquire(&mutex_robotStarted, TM_INFINITE);
            robotStarted = 1;
            rt_mutex_release(&mutex_robotStarted);
        }
    }
}


/**
 * @brief Thread handling control of the robot.
 */
[[noreturn]] void Tasks::MoveTask(void *arg) {
    int rs;
    int cpMove;
    
    cout << "Start " << __PRETTY_FUNCTION__ << endl << flush;
    // Synchronization barrier (waiting that all tasks are starting)
    rt_sem_p(&sem_barrier, TM_INFINITE);
    
    /**************************************************************************************/
    /* The task starts here                                                               */
    /**************************************************************************************/
    rt_task_set_periodic(nullptr, TM_NOW, 100000000);

    while (true) {
        rt_task_wait_period(nullptr);

        rt_mutex_acquire(&mutex_robotConnected, TM_INFINITE);
        bool rc = robotConnected;
        rt_mutex_release(&mutex_robotConnected);

        if(!rc) continue; // WHILE THE ROBOT IS NOT CONNECTED WE DO NOTHING

        cout << "Periodic movement update";
        rt_mutex_acquire(&mutex_robotStarted, TM_INFINITE);
        rs = robotStarted;
        rt_mutex_release(&mutex_robotStarted);
        if (rs == 1) {
            rt_mutex_acquire(&mutex_move, TM_INFINITE);
            cpMove = move;
            rt_mutex_release(&mutex_move);
            
            cout << " move: " << cpMove;

            this->WriteToRobot(new Message((MessageID)cpMove));
        }
        cout << endl << flush;
    }
}

/**
 * Write a message in a given queue
 * @param queue Queue identifier
 * @param msg Message to be stored
 */
void Tasks::WriteInQueue(RT_QUEUE *queue, Message *msg) {
    int err;
    if ((err = rt_queue_write(queue, (const void *) &msg, sizeof ((const void *) &msg), Q_NORMAL)) < 0) {
        cerr << "Write in queue failed: " << strerror(-err) << endl << flush;
        throw std::runtime_error{"Error in write in queue"};
    }
}

/**
 * Read a message from a given queue, block if empty
 * @param queue Queue identifier
 * @return Message read
 */
Message *Tasks::ReadInQueue(RT_QUEUE *queue) {
    int err;
    Message *msg;

    if ((err = rt_queue_read(queue, &msg, sizeof ((void*) &msg), TM_INFINITE)) < 0) {
        cout << "Read in queue failed: " << strerror(-err) << endl << flush;
        throw std::runtime_error{"Error in read in queue"};
    }/** else {
        cout << "@msg :" << msg << endl << flush;
    } */

    return msg;
}

Message *Tasks::WriteToRobot(Message *msg) {
    Message *msgRet = nullptr;

    rt_mutex_acquire(&mutex_robot, TM_INFINITE);
    msgRet = robot.Write(msg);
    rt_mutex_release(&mutex_robot);

    /* CHECK IF CONNECTION STILL ALIVE */

    if (msgRet->CompareID(MESSAGE_ANSWER_COM_ERROR) ||
            msgRet->CompareID(MESSAGE_ANSWER_ROBOT_TIMEOUT)) {
        if (++this->robot_err_counter >= RETRY_ERR_ROBOT+1) { // if too many errors, close connection and display error
            WriteInQueue(&q_messageToMon, msgRet);
            this->CloseRobot();
            // reopen
            rt_sem_v(&sem_openComRobot);
            this->robot_err_counter = RETRY_ERR_ROBOT+2; // in order to avoid overflow we just restrict the value to +2
        }
        msgRet = new Message(MESSAGE_ANSWER_NACK);
    } else {
        this->robot_err_counter = 0;
        rt_mutex_acquire(&mutex_robotConnected, TM_INFINITE);
        robotConnected = true;
        rt_mutex_release(&mutex_robotConnected);
    }

    return msgRet;
}

[[noreturn]] void Tasks::BatteryTask(void *arg) {
    bool rc;
    int cpMove;

    cout << "Start " << __PRETTY_FUNCTION__ << endl << flush;
    // Synchronization barrier (waiting that all tasks are starting)
    rt_sem_p(&sem_barrier, TM_INFINITE);

    /**************************************************************************************/
    /* The task starts here                                                               */
    /**************************************************************************************/
    rt_task_set_periodic(nullptr, TM_NOW, 500000000); // periode de 500 ms entre chaque actualisation

    while (true) {
        rt_task_wait_period(nullptr);
        rt_mutex_acquire(&mutex_robotConnected, TM_INFINITE);
        rc = robotConnected;
        rt_mutex_release(&mutex_robotConnected);
        if (rc) {
            Message *msgBattery = this->WriteToRobot(ComRobot::GetBattery());

            cout << "Periodic battery update: " << msgBattery->ToString() << endl << flush;
            WriteInQueue(&q_messageToMon, msgBattery);
        }
    }
}

[[noreturn]] void Tasks::WatchDogTask(void *arg) {
    cout << "Start " << __PRETTY_FUNCTION__ << endl << flush;
    // Synchronization barrier (waiting that all tasks are starting)
    rt_sem_p(&sem_barrier, TM_INFINITE);
    bool rc = true;

    /**************************************************************************************/
    /* The task startRobot starts here                                                    */
    /**************************************************************************************/

    rt_task_set_periodic(nullptr, TM_NOW, 1000000000); // periode de 1 s entre chaque actualisation
    while (true) {
        Message * msgSend;
        rt_sem_p(&sem_startWithWD, TM_INFINITE);
        cout << "Start robot with watchdog (";
        msgSend = this->WriteToRobot(ComRobot::StartWithWD());
        cout << msgSend->GetID();
        cout << ")" << endl;

        cout << "Movement answer: " << msgSend->ToString() << endl << flush;
        WriteInQueue(&q_messageToMon, msgSend);  // msgSend will be deleted by sendToMon

        if (msgSend->GetID() == MESSAGE_ANSWER_ACK) {
            rt_mutex_acquire(&mutex_robotStarted, TM_INFINITE);
            robotStarted = 1;
            rt_mutex_release(&mutex_robotStarted);
        }

        rt_mutex_acquire(&mutex_robotConnected, TM_INFINITE);
        rc = robotConnected;
        rt_mutex_release(&mutex_robotConnected);
        while (rc) {
            rt_task_wait_period(nullptr);
            this->WriteToRobot(ComRobot::ReloadWD());
            rt_mutex_acquire(&mutex_robotConnected, TM_INFINITE);
            rc = robotConnected;
            rt_mutex_release(&mutex_robotConnected);
        }
    }
}