#include "ServerFrameLockAccept.h"
#include "SimpleThread.h"
#include "UtilDef.h"
#include <stdio.h>
#include <string.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <stdlib.h>
//#include "ErroLog.h"
namespace Husky
{
    using namespace simpleThread;

    bool CServerFrame::m_bShutdown = false;

    PM CServerFrame::m_pmAccept; 

    CServerFrame::CServerFrame(void)
    {

    }

    CServerFrame::~CServerFrame(void)
    {
        pthread_mutex_destroy(&m_pmAccept);
    }

    bool CServerFrame::CloseServer()
    {
        m_bShutdown=true;
        if (SOCKET_ERROR==closesocket(m_lsnSock))
        {
            LogError("file:%s , line: %d, error info: %s\n",__FILE__,__LINE__,strerror(errno));
            return false;
        }


        int sockfd;
        struct sockaddr_in dest;

        if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        {
            LogError("file:%s , line: %d, error info: %s\n",__FILE__,__LINE__,strerror(errno));
            return false;
        }

        bzero(&dest, sizeof(dest));
        dest.sin_family = AF_INET;
        dest.sin_port = htons(m_nLsnPort);
        if (inet_aton("127.0.0.1", (struct in_addr *) &dest.sin_addr.s_addr) == 0)
        {
            LogError("file:%s , line: %d, error info: %s\n",__FILE__,__LINE__,strerror(errno));
            return false;
        }

        if (connect(sockfd, (struct sockaddr *) &dest, sizeof(dest)) < 0)
        {
            fprintf(stdout,"file:%s , line: %d, error info: %s\n",__FILE__,__LINE__,strerror(errno));
        }
        close(sockfd);
        return true;

    }

    bool CServerFrame::RunServer()
    {
        if(SOCKET_ERROR==listen(m_lsnSock,LISEN_QUEUR_LEN))
        {
            LogError("file:%s , line: %d, error info: %s\n",__FILE__,__LINE__,strerror(errno));
            return false;
        }	
        threadManager thrMngr;	
        int i;
        SPara para;
        para.hSock=m_lsnSock;
        para.pHandler=m_pHandler;
        for (i=0;i<m_nThreadCount;i++)
        {		
            if (0!=thrMngr.CreateThread(ServerThread, &para))
            {
                break;	
            }		
        }	
        printf("expect thread count %d, real count %d\n",m_nThreadCount,i);
        if(i==0)	
        {
            LogError("file:%s , line: %d, error info: %s\n",__FILE__,__LINE__,strerror(errno));
            return false;
        }

        printf("server start to run.........\n");

        if (thrMngr.WaitMultipleThread()!=0)
        {
            return false;//�ȴ������߳��˳�
        }
        printf("server shutdown ok............\n");
        return true;

    }

    void* CServerFrame::ServerThread(void *lpParameter )
    {
        SPara *pPara=(SPara*)lpParameter;
        SOCKET hSockLsn=pPara->hSock;
        SRequestHandler *pHandler=pPara->pHandler;
        int nRetCode;
        linger lng;
        char chRecvBuf[RECV_BUFFER];

        SOCKET hClientSock;
        string strHttpXml;
        while(!m_bShutdown)
        {		
            pthread_mutex_lock(&m_pmAccept);
            hClientSock=accept(hSockLsn,NULL,NULL);
            pthread_mutex_unlock(&m_pmAccept);
            if(hClientSock==SOCKET_ERROR)
            {
                if(!m_bShutdown)
                  LogError("file:%s , line: %d, error info: %s\n",__FILE__,__LINE__,strerror(errno));
                continue;
            }
            //printf("start to serve:id = %d\n",nSockNum);

            //printf("server thread id = %d,socket id = %d",nSockNum,hClientSock);

            lng.l_linger=1;
            lng.l_onoff=1;				
            if(SOCKET_ERROR==setsockopt(hClientSock,SOL_SOCKET,SO_LINGER,(char*)&lng,sizeof(lng)))			
            {
                LogError("file:%s , line: %d, error info: %s\n",__FILE__,__LINE__,strerror(errno));
            }

            struct timeval to;
            to.tv_sec=5;
            to.tv_usec=0;

            struct timeval in;
            in.tv_sec=5;
            in.tv_usec=0;

            if(SOCKET_ERROR==setsockopt(hClientSock,SOL_SOCKET,SO_RCVTIMEO,(char*)&in,sizeof(in)))
              LogError("file:%s , line: %d, error info: %s\n",__FILE__,__LINE__,strerror(errno));
            if(SOCKET_ERROR==setsockopt(hClientSock,SOL_SOCKET,SO_SNDTIMEO,(char*)&to,sizeof(to)))
              LogError("file:%s , line: %d, error info: %s\n",__FILE__,__LINE__,strerror(errno));


            string strRec;  //�������ݻ�����
            string strSnd;            //�������ݻ�����
            while(true)
            {
                memset(chRecvBuf,0,sizeof(chRecvBuf));//ÿ�γ�ʼ���ִ�		
                nRetCode=recv(hClientSock,chRecvBuf,RECV_BUFFER-1,0);	
                if(nRetCode>0)
                {
                    strRec+=chRecvBuf;
                    if(strstr(chRecvBuf," HTTP")!=NULL)
                      break;
                }
                else
                  break;
            }


            if(SOCKET_ERROR==nRetCode)
            {
                LogError("error info: %s",strerror(errno));
                closesocket(hClientSock);
                continue;
            }
            if(0==nRetCode)
            {
                LogInfo("****connection has been gracefully closed");/*comment by msdn*/
                closesocket(hClientSock);
                continue;
            }

            (*pHandler)(strRec,strSnd);     
            char chHttpHeader[1024];


            //sprintf(chHttpHeader,   "HTTP/1.1 200 OK\r\n"
            //            "Connection: close\r\n"
            //            "Server: FrameServer/1.0.0\r\n"  
            //            "Content-Type: text/xml; charset=%s\r\n"
            //            "Content-Length: %d\r\n\r\n",RESPONSE_CHARSET_UTF8,strSnd.length());
            sprintf(chHttpHeader, RESPONSE_FORMAT, RESPONSE_CHARSET_UTF8, strSnd.length());
            LogDebug(chHttpHeader);
            strHttpXml=chHttpHeader;
            strHttpXml+=strSnd;

            if (SOCKET_ERROR==send(hClientSock,strHttpXml.c_str(),strHttpXml.length(),0))
            {
                LogError("file:%s , line: %d, error info: %s\n",__FILE__,__LINE__,strerror(errno));
            }
            closesocket(hClientSock);
        }

        //			g_threadStatusArray[threadNum]=CLIENT_SOCKET_CLOSED;//�ر�����
        return 0;

    }//while(!g_fShutdown)

    bool CServerFrame::CreateServer(u_short nPort,u_short nThreadCount,SRequestHandler *pHandler)
    {

        m_nLsnPort=nPort;
        m_nThreadCount=nThreadCount;
        m_pHandler=pHandler;


        if (!BindToLocalHost(m_lsnSock,m_nLsnPort))
        {
            return false;
        }
        pthread_mutex_init(&m_pmAccept,NULL);

        return true;
    }

    bool  CServerFrame::BindToLocalHost(SOCKET &sock,u_short nPort)
    {
        sock=socket(AF_INET,SOCK_STREAM,0);
        if(INVALID_SOCKET==sock)
        {
            LogError("file:%s , line: %d, error info: %s\n",__FILE__,__LINE__,strerror(errno));
            return false;
        }

        /* ʹ��ַ���Ͽ������� */
        int nRet = 1;
        if(SOCKET_ERROR==setsockopt(m_lsnSock, SOL_SOCKET, SO_REUSEADDR, (char*)&nRet, sizeof(nRet)))
        {	
            LogError("file:%s , line: %d, error info: %s\n",__FILE__,__LINE__,strerror(errno));
        }

        struct sockaddr_in addrSock;
        addrSock.sin_family=AF_INET;
        addrSock.sin_port=htons(nPort);
        addrSock.sin_addr.s_addr=htonl(INADDR_ANY);
        int retval;
        retval=bind(sock,(sockaddr*)&addrSock,sizeof(sockaddr));
        if(SOCKET_ERROR==retval)
        {
            LogError("file:%s , line: %d, error info: %s\n",__FILE__,__LINE__,strerror(errno));
            closesocket(sock);
            return false;
        }

        return true;

    }
}