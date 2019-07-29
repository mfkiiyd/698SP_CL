#include "serial.h"
#include "ui_serial.h"
#include "Qt"
#include <thread>
#include <QtWidgets/QMessageBox>
#include <QList>
#include <QDebug>

using namespace std;
#pragma comment (lib, "ws2_32")
#pragma comment (lib, "winmm")
#define NAME_LEN 500

extern int Stringlist2Hex_char(QString &str, char *pOut);
extern int String2Hex(string &str, BYTE *pOut, DWORD *pLenOut, DWORD nMaxLen);

extern string StringAddSpace(string &input);

extern string StringAddSpace(QString &input);

extern int check(QString);


Serial::Serial(QWidget *parent) :
        QDialog(parent),
        ui(new Ui::Dialog)
{
    ui->setupUi(this);
    setWindowTitle("通讯配置");
    setWindowFlags(Qt::WindowStaysOnTopHint | Qt::MSWindowsFixedSizeDialogHint);
    connect(ui->pushButton, SIGNAL(clicked()), this, SLOT(creat_process()));
    connect(this, SIGNAL(open_fail_message()), this, SLOT(warming()));
    connect(this, SIGNAL(send_write(QString)), this, SLOT(write(QString)));
    QList<QString> PortList;
    PortList = getEnableCommPort(PortList);
    for (int i = 0; i < PortList.size(); i++)
    {
        ui->comboBox->addItem(PortList[i]);
    }
}

void Serial::creat_process()
{
    if (ui->radioButton->isChecked())  //串口
    {
        if (ui->pushButton->text() == "打开")
        {
            ui->pushButton->setText("关闭");
            ui->comboBox->setDisabled(1);
            ui->comboBox_2->setDisabled(1);
            ui->lineEdit->setDisabled(1);
            ui->radioButton_2->setDisabled(1);
            QString str = ui->comboBox->currentText();
            char *ch;
            QByteArray ba = str.toLatin1();
            ch = ba.data();
            std::basic_string<TCHAR> s1 = ch;
            std::thread t1(&Serial::open_serial, this, s1);
            t1.detach();
            close();
            internet_or_serial = 1;
        } else
        {
            ui->pushButton->setText("打开");
            ui->comboBox->setDisabled(0);
            ui->comboBox_2->setDisabled(0);

            ui->lineEdit->setDisabled(0);
            ui->radioButton_2->setDisabled(0);
            CloseSerial();
        }

    } else
    {
        if (ui->pushButton->text() == "打开")
        {
            ui->pushButton->setText("关闭");
            std::thread t2(&Serial::build_net, this);
            t2.detach();
            close();
            internet_or_serial = 0;
        } else
        {
            ui->pushButton->setText("打开");
            ui->comboBox->setDisabled(0);
            ui->comboBox_2->setDisabled(0);
            ui->lineEdit->setDisabled(0);
            ui->radioButton_2->setDisabled(0);
        }
    }
}


bool Serial::open_serial(std::basic_string<TCHAR> s1)
{
    char str[1000] = {0};
    DWORD wCount;//读取的字节数
    BOOL bReadStat;
    std::basic_string<TCHAR> s2 = R"(\\.\)";
    std::basic_string<TCHAR> s3 = s2 + s1;
    LPCTSTR a3 = s3.c_str();
    hCom = CreateFile(a3,
                      GENERIC_READ | GENERIC_WRITE, //允许读和写
                      0, //独占方式
                      NULL,
                      OPEN_EXISTING, //打开而不是创建
                      0, //同步方式
                      NULL);
    SetupComm(hCom, 1024, 1024); //输入缓冲区和输出缓冲区的大小都是1024
    if (hCom == (HANDLE) -1)
    {
        cout << "open com fail!";
        emit open_fail_message();
        return FALSE;
    }
    run_flag = true;
    COMMTIMEOUTS TimeOuts;
    TimeOuts.ReadIntervalTimeout = 0;
    TimeOuts.ReadTotalTimeoutMultiplier = 0;
    TimeOuts.ReadTotalTimeoutConstant = 200;
    TimeOuts.WriteTotalTimeoutMultiplier = 0;
    TimeOuts.WriteTotalTimeoutConstant = 0;
    SetCommTimeouts(hCom, &TimeOuts); //设置超时
    DCB dcb;
    GetCommState(hCom, &dcb);
    dcb.BaudRate = (unsigned) ui->comboBox_2->currentText().toLong(nullptr);
    dcb.ByteSize = 8;
    dcb.Parity = EVENPARITY;
    dcb.StopBits = ONESTOPBIT;
    SetCommState(hCom, &dcb);
    PurgeComm(hCom, PURGE_TXCLEAR | PURGE_RXCLEAR);//清空缓冲区
    QString add = "6817004345AAAAAAAAAAAA10DA5F0501034001020000900f16";
    emit send_write(add);
    PurgeComm(hCom, PURGE_TXABORT |
                    PURGE_RXABORT | PURGE_TXCLEAR | PURGE_RXCLEAR);
    string temp1 = "";
    int times = 0;
    while (run_flag)
    {
        bReadStat = ReadFile(hCom, str, 700, &wCount, nullptr);
        if (!bReadStat)
        {
            cout << "Read data fail!";
            return FALSE;
        } else
        {
            string output = "";
            char temp[10] = {0};
            for (int i = 0; i < wCount; i++)
            {
                sprintf(temp, "%02X ", (BYTE) str[i]);
                output = output + temp;
            }
            if (output == "")
            {
                continue;
            }
            if (temp1 != "")
            {
                output = temp1 + output;
                times += 1;
                if (times > 2500)
                {
                    temp1 = "";
                    times = 0;
                    continue;
                }
            }
            switch (check(QString::fromStdString(output)))
            {
                case 0:
                {
//                    qDebug()<<"case 0";
                }
                    break;
                case 1:
                {
                    cout << "Receive: " << output << endl;
                    emit receive_message(QString::fromStdString(output));   //接收显示
                    PurgeComm(hCom, PURGE_TXABORT |
                                    PURGE_RXABORT | PURGE_TXCLEAR | PURGE_RXCLEAR);
                    temp1 = "";
                }
                    break;
                case 2:
                {
//                    qDebug()<<"case 2";
                    temp1 = output;
                }
                    break;
                default:
//                    qDebug()<<"default";
                    break;
            }
        }
    }
    cout << "quit cir\n";
    CloseHandle(hCom);
    return 0;
}

bool Serial::write(QString add)
{
//    qDebug()<<"send_message->add"<<add;
    std::thread t3(&Serial::write_, this, add);
    t3.detach();
}

bool Serial::write_(QString add) //发送
{
    if (internet_or_serial == 1) //串口
    {
        BYTE Apdu[1500] = {0};
        DWORD nApduLen = 0;
        string new_add = add.toStdString();
        transform(new_add.begin(), new_add.end(), new_add.begin(), ::tolower);
        String2Hex(new_add, Apdu, &nApduLen, sizeof(Apdu));
        COMSTAT ComStat;
        DWORD dwErrorFlags;
        BOOL bWriteStat;
        ClearCommError(hCom, &dwErrorFlags, &ComStat);
        DWORD dwBytesWrite = nApduLen;
        cout << "Send: " << StringAddSpace(new_add) << endl;
        emit send_message(add);
        bWriteStat = WriteFile(hCom, Apdu, dwBytesWrite, &dwBytesWrite, NULL);
        if (!bWriteStat)
        {
            cout << "Write data fail!!" << endl;
        }

    } else  //以太网
    {
        char Apdu[1500] = {0};
        Stringlist2Hex_char(add, Apdu);
        send(clientSock, Apdu, add.length() / 2, 0);
        string new_add = add.toStdString();
        cout << "Eth Send: " << StringAddSpace(new_add) << endl;
        emit send_message(add);

    }
    return true;
}

bool Serial::build_net()
{
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
    //这个是服务器的socket，用来绑定网卡来监听
    SOCKET s = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    sockaddr_in addr;
    addr.sin_addr.S_un.S_addr = inet_addr("0.0.0.0");
    addr.sin_port = htons(20001);
    addr.sin_family = PF_INET;
    bind(s, (SOCKADDR *) &addr, sizeof(SOCKADDR));
    listen(s, 1);
    SOCKADDR clientAddr;
    int nSize = sizeof(SOCKADDR);
    //这个socket用来接收客户端发过来的信息
    clientSock = ::accept(s, &clientAddr, &nSize);
    while (run_flag)
    {
        cout << "running eth\n";
        char buff[1400] = {0};
        //接收客户端发来的命令
        int i = 1;
        i = recv(clientSock, buff, 1400, 0);
        int ti = 1400 - 1;
        int qwe = 0;
        while (true)
        {
            if (buff[ti] == '\000')
            {
                qwe += 1;
                ti -= 1;
            } else
                break;
            continue;
        }
        char a[100] = {0};
        string output = "";
        for (int x = 0; x < 1400 - qwe; x++)
        {
            sprintf(a, "%02X ", (BYTE) buff[x]);
            output = output + a;
        }
        if (i != -1)
        {
            cout << "Receive from client :" << output << endl;

            emit receive_message(QString::fromStdString(output));
        }


    }
    closesocket(clientSock);
    closesocket(s);
    WSACleanup();
    return true;
}

BOOL Serial::CloseSerial()
{
    run_flag = false;
}

QStringList Serial::getEnableCommPort(QList<QString> &PortList)
{
    PortList.clear();
    HKEY hkey;
    LONG lRes = RegOpenKeyEx(HKEY_LOCAL_MACHINE, _T("HARDWARE\\DEVICEMAP\\SERIALCOMM"), 0,
                             KEY_QUERY_VALUE | KEY_ENUMERATE_SUB_KEYS | KEY_READ, &hkey);
    if (lRes == ERROR_SUCCESS)
    {
        TCHAR tchKey[MAX_PATH];
        TCHAR tchValue[20];
        DWORD dwIndex = 0;
        DWORD dwType = REG_SZ;
        while (lRes == ERROR_SUCCESS)
        {
            DWORD dwCount = MAX_PATH;
            DWORD dwVCount = 20;
            lRes = RegEnumValue(hkey, dwIndex++, tchKey, &dwCount, NULL, &dwType, (LPBYTE) tchValue, &dwVCount);
            if (lRes == ERROR_SUCCESS)
            {
                if ((dwVCount > 0) && (dwCount > 0))
                {
                    QString str;
                    str = tchValue;
                    PortList.append(str);
                }
            }
        }
    }
    RegCloseKey(hkey);
//    qDebug() << "Date:" << PortList;
    return PortList;
}


Serial::~Serial()
{}

void Serial::warming()
{
    QMessageBox::warning(this, "ERROR", "串口打开失败!", QMessageBox::Ok);
    ui->pushButton->setText("打开");
    ui->comboBox->setDisabled(0);
    ui->comboBox_2->setDisabled(0);
}

