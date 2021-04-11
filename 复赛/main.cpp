#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <map>
#include<set>
#include <vector>
#include <time.h>
#include <algorithm>
#include <assert.h>

using namespace std;

// 不做初始化，如果创建虚拟机请求，得不到满足，动态增加服务器，已修复test6的bug,为服务器重新编号，批量输出
// 虚拟机需求从大到小排列，依次满足需求
// 增加迁移操作,迁移时服务器按使用率排序,需要迁移的服务器内的虚拟机降序排序,放置虚拟机时按放置后最大利用率放
// 修改利用率公式
// 修改 SERVERCMP
// **购买服务器时，TOP-K贪心 K=8，MaxCostDet=50000,4*cpu+mem**
//#define UPLOAD

#define TEST

#ifdef TEST
const string filePath = "../training-data/training-2.txt";
#endif // TEST

struct SERVER{
    /*服务器基本属性*/
    string serverType;      //服务器型号
    int cpuCores,memorySize,serverCost,powerCost;  //cpu核数，内存，硬件成本，能耗成本

    /*服务器状态属性*/
    int ID=-1;               //服务器 ID
    int state=0,runvmcnt=0;  //是否开机,运行的虚拟机数量
    int A_cpuCores,A_memorySize,B_cpuCores,B_memorySize;  //A,B分区的 CPU,MEM剩余空间
    set<int>  vmidset;                               //服务器内装有的虚拟机ID集合
    SERVER(){}
    SERVER(string m_serverType,int m_cpuCores,int m_memorySize,int m_serverCost,int m_powerCost ){
        serverType=m_serverType;
        cpuCores=m_cpuCores,memorySize=m_memorySize,serverCost=m_serverCost,powerCost=m_powerCost;
        A_cpuCores = m_cpuCores/2, B_cpuCores = m_cpuCores/2;
        A_memorySize= m_memorySize/2, B_memorySize= m_memorySize/2;
    }
};

int remain_days;
bool SERVERCMP(SERVER &A , SERVER &B){
    return remain_days*(A.powerCost)+A.serverCost < remain_days*(B.powerCost)+B.serverCost;
    //return A.powerCost<B.powerCost;
    //return A.serverCost < B.serverCost;
    //return remain_days*(A.powerCost-0.3*A.cpuCores)+A.serverCost-100*A.cpuCores < remain_days*(B.powerCost-0.3*B.cpuCores)+B.serverCost-100*B.cpuCores;
}

struct VM{
    string vmType;         // 虚拟机型号
    int cpuCores, memorySize, isDouble;           //cpu核数，内存，是否双节点部署

    /*虚拟机状态属性*/
    int ID=-1;
    VM(){}
    VM(string m_vmType, int m_cpuCores, int m_memorySize, int m_isDouble){
        vmType=m_vmType;
        cpuCores=m_cpuCores, memorySize=m_memorySize, isDouble=m_isDouble;
    }
};

bool VMCMP(VM &A ,VM & B){
    //return abs(A.cpuCores-A.memorySize) < abs(B.cpuCores-B.memorySize);
    return A.cpuCores+A.memorySize>B.cpuCores+B.memorySize;
}


struct SERVERCOMB{
    int ServerIdx;   //服务器IDX
    int powerCost;   // 服务器能耗成本
    double value;   //排序指标的值

    SERVERCOMB(){}
    SERVERCOMB(int m_ServerIdx, int m_powerCost, double m_value){
        ServerIdx=m_ServerIdx;powerCost=m_powerCost;value=m_value;
    }
};

bool SERVERCOMBCMP(SERVERCOMB& A, SERVERCOMB& B){
    if(A.value==B.value){
        return A.powerCost<B.powerCost;
    }
    return A.value>B.value;
}


struct VMCOMB{
    int vmid,cpuCores, memorySize;
    VMCOMB(){}
    VMCOMB(int m_vmid, int m_cpuCores, int m_memorySize){
        vmid=m_vmid;cpuCores=m_cpuCores;memorySize=m_memorySize;
    }
};

bool VMCOMBCMP(VMCOMB& A, VMCOMB& B){
    return A.cpuCores+A.memorySize<B.cpuCores+B.memorySize;
}

/****************公有变量区************************/
/*服务器，虚拟机基本信息*/
vector<SERVER> AllServer;
vector<VM> AllVm;
unordered_map<string,int> serverType2idx, vmType2idx;

/*请求记录*/
vector<vector<int>> Request[1010]; //记录每天的请求，数组小标表示day，[[0,创建虚拟机ID],[1,删除虚拟机ID]]

unordered_map<int,int> vmid2vmtypeidx;      //虚拟机ID 到 虚拟机类型表的idx映射

/*服务器数量限制*/
int MAXSERVERCNT=100000;
int MAXDAYS=1000;

/*虚拟机迁移比率*/
double MigrateRate=0.03;

/*输入数目变量*/
int serverNum,vmNumber;  //服务器类型数目,虚拟机类型数量
int requestdays = 0, intervaldays=0,dayRequestNumber = 0;   // 天数，间隔天数，每天请求量

/****************************************/

/*历史cpu、mem的峰值*/
int MAXCPU=0,MAXMEM=0;
int totcpu=0,totmem=0;                       // cpu,mem使用量记录

/*贪心TOP-K*/
int greedy_k=8;

class IO{
public:

static void  input(){

#ifdef TEST
    std::freopen(filePath.c_str(),"rb",stdin);
#endif
    /**输入所有类型的服务器**/
    scanf("%d\n",&serverNum);   //服务器类型数目
    AllServer.resize(serverNum);
    for(int i=0; i<serverNum; i++){
        string LINE;
        getline(cin,LINE);
        LINE = LINE.substr(1,LINE.size()-2);
        vector<string> LineSplit;
        split(LINE,LineSplit,',');
        string serverType=LineSplit[0];
        int cpuCores=stoi(LineSplit[1]),memorySize=stoi(LineSplit[2]),serverCost=stoi(LineSplit[3]),powerCost=stoi(LineSplit[4]);
        AllServer[i]=SERVER(serverType,cpuCores,memorySize,serverCost,powerCost);
    }

        //按成本从低到高排序
    sort(AllServer.begin(),AllServer.end(),SERVERCMP);
    for(int i=0;i<AllServer.size();i++){
        string serverType=AllServer[i].serverType;
        serverType2idx[serverType]=i;
    }


    /**输入所有类型的虚拟机**/
    scanf("%d\n",&vmNumber);  //虚拟机类型数量
    AllVm.resize(vmNumber);
    for(int i =0; i<vmNumber; i++){
        string LINE;
        getline(cin,LINE);
        LINE = LINE.substr(1,LINE.size()-2);
        vector<string> LineSplit;
        split(LINE,LineSplit,',');

        string vmType=LineSplit[0];
        int cpuCores=stoi(LineSplit[1]), memorySize=stoi(LineSplit[2]), isDouble=stoi(LineSplit[3]);
        //cout<<"vmType="<<vmType<<endl;
        AllVm[i]=VM(vmType,cpuCores,memorySize,isDouble);
        vmType2idx[vmType]=i;
    }
}

static void  inputRequestDay(){
    /*输入请求天数，间隔天数*/
    scanf("%d%d\n",&requestdays,&intervaldays);
}

static void  inputRequest(int a, int b){
    /**
    * function 读入 [a,b)的请求
    */
    for(int i=a; i<b; i++){
        scanf("%d\n",&dayRequestNumber);
        for(int j=0; j<dayRequestNumber; j++){

            string LINE;
            getline(cin,LINE);
            LINE = LINE.substr(1,LINE.size()-2);
            vector<string> LineSplit;
            split(LINE,LineSplit,',');
            if(LineSplit.size()==3)
            {
                // add
                string vmType=LineSplit[1].substr(1);
                int VmID=stoi(LineSplit[2]);
                //cout<<"VmID= "<<VmID<<endl;
                int vmtypeidx = vmType2idx[vmType];
                //cout<<"vmtypeidx="<<vmtypeidx<<endl;
                Request[i].push_back(vector<int>{0,VmID});
                vmid2vmtypeidx[VmID]=vmtypeidx;
                //cout<<"vmType="<<vmType<<" VmID="<<VmID<<" vmtypeidx="<<vmtypeidx<<endl;
                totcpu = totcpu + AllVm[vmtypeidx].cpuCores;   MAXCPU=max(MAXCPU,totcpu);
                totmem = totmem + AllVm[vmtypeidx].memorySize; MAXMEM=max(MAXMEM,totmem);
            }
            else
            {
                // del
                int VmID=stoi(LineSplit[1]);
                Request[i].push_back(vector<int>{1,VmID});
                int vmtypeidx = vmid2vmtypeidx[VmID];
                totcpu = totcpu - AllVm[vmtypeidx].cpuCores;
                totmem = totmem - AllVm[vmtypeidx].memorySize;
            }
        }
    }

}

/********工具类函数********/
static void  split(const string& s,vector<string>& sv,const char flag = ' '){
    sv.clear();
    istringstream iss(s);
    string temp;

    while (getline(iss, temp, flag))
    {
        sv.push_back(temp);
    }
    return;
}
/***************************/
};


class Solver{

public:
    /*当前的天数，第几天*/
    int CurDay;

    /*当前服务器集群放置了多少台虚拟机*/
    int CurVMCnt;

    /*购买的服务器状态*/
    int PurServerCnt;              //购买服务器的数量
    vector<SERVER> PurServer;        //购买的服务器的服务器列表

    /*虚拟机位置记录信息*/
    unordered_map<int,vector<int>> vmid2loc;    //虚拟机ID 到 虚拟机所在服务器位置(ID,A/B分区)的映射 <VMID,(SERVERID,A/B)>

    /*随机种子*/
    int seed;

    /*最大失败次数*/
    int max_failure_cnt;

    /*输出*/
    long long SERVERCOST = 0, POWERCOST=0, TOTALCOST =0;  // 成本
    vector<vector<string> > ExpanRes,MigrateRes,CreateVmRes;  // 每天的创建服务器，扩容，迁移 操作输出序列


    Solver(int m_seed,int m_failure_cnt=0){
        seed=m_seed;
        srand(seed);
        max_failure_cnt=m_failure_cnt;
        PurServer.resize(MAXSERVERCNT+10);
        ExpanRes.resize(MAXDAYS+10);MigrateRes.resize(MAXDAYS+10);CreateVmRes.resize(MAXDAYS+10);
        PurServerCnt=0;CurDay=0;CurVMCnt=0;
    }

    // 扩容服务器策略
    void expansion(vector<int>& servertype2cnt){
        OutputExpansion(servertype2cnt);
    }

    // 迁移虚拟机策略
    void migrate(){

        unordered_map<int,vector<int>> vmid2mvloc;          //记录虚拟器迁移位置 <VMID,(SERVERID,A/B)>
        int MaxMigrateNum=MigrateRate*CurVMCnt;   //最大迁移虚拟机数量
        //printf("MaxMigrateNum=%d\n",MaxMigrateNum);

        if(MaxMigrateNum==0){
            OutputMigrate(vmid2mvloc);
            return;
        }

        /*服务器按存放的虚拟机个数排序*/
        vector<SERVERCOMB> PurComb;
        for(int i=0;i<PurServerCnt;i++){
            SERVER& server=PurServer[i];
            double value = 1-(double(server.A_cpuCores+server.A_memorySize+server.B_cpuCores+server.B_memorySize))/(server.cpuCores+server.memorySize);
            //value = value*(double(1)/(server.A_cpuCores+server.A_memorySize+server.B_cpuCores+server.B_memorySize));
            SERVERCOMB c(i,server.powerCost,value);
            PurComb.push_back(c);
        }
        sort(PurComb.begin(),PurComb.end(),SERVERCOMBCMP);

        //遍历迁移
        int mvcnt=0;     //记录迁移的虚拟机数量
        int failure_cnt=0;  //迁移失败次数
        for(int i=PurServerCnt-1;i>=0;i--){
            //printf("i=%d\n",i);
            int SourceServerIdx=PurComb[i].ServerIdx;

            set<int> vmidset=PurServer[SourceServerIdx].vmidset;
            //服务器内的虚拟机排序
            vector<VMCOMB> LoadVMComb;
            for(int vmid:vmidset){
                int vmtypeidx = vmid2vmtypeidx[vmid];
                VM& v = AllVm[vmtypeidx];
                int cpuCores=v.cpuCores, memorySize=v.memorySize;
                VMCOMB c(vmid,cpuCores,memorySize);
                LoadVMComb.push_back(c);
            }

            sort(LoadVMComb.begin(),LoadVMComb.end(),VMCOMBCMP);

            int is_success=1;
            for(VMCOMB& c: LoadVMComb){
                int vmid=c.vmid;
                int flag=0;       //此台虚拟机是否迁移成功

                double max_use_rate=0.0;
                int flag_m=-1,flag_serveridx=-1;

                int vmtypeidx = vmid2vmtypeidx[vmid];
                VM v = AllVm[vmtypeidx]; v.ID=vmid;

                for(int j=0;j<i;j++){
                    if(mvcnt>=MaxMigrateNum) break;
                    int TargertServerIdx=PurComb[j].ServerIdx;
                    SERVER& server=PurServer[TargertServerIdx];
                    int m=-1;
                    double use_rate=-1;
                    match(server, v, m, use_rate);

                    if(m!=-1&&max_use_rate<use_rate){
                        max_use_rate=use_rate;
                        flag_m=m;
                        flag_serveridx=TargertServerIdx;
                    }
                }

                if(flag_m!=-1){
                    MoveVM(vmid,SourceServerIdx,flag_serveridx,flag_m);  //迁移服务器
                    mvcnt++;flag=1;
                    //vmid2loc[vmid]=vector<int>{flag_serveridx,flag_m};
                    vmid2mvloc[vmid]=vector<int>{flag_serveridx,flag_m};
#ifdef TEST
                    if(vmid==-1) printf("====================================\n");
#endif // TEST
                    //printf("**********Move***********\n");
                }
                //if(mvcnt>=MaxMigrateNum) {is_success=0;break;}
                //if(mvcnt>=MaxMigrateNum||!flag) {is_success=0;break;}
                if(flag==0) {failure_cnt++;break;}
                if(mvcnt>=MaxMigrateNum||failure_cnt>max_failure_cnt) {is_success=0;break;}
            }

            if(mvcnt>=MaxMigrateNum||!is_success) break;
        }

        OutputMigrate(vmid2mvloc);

    }

    /*void ZeroMigrate(){
        string s = "(migration, 0)\n";
        MigrateRes[CurDay].push_back(s);
    }*/

    // 服务器、虚拟机匹配
    void match(SERVER &server, VM& v, int& ret_m, double& ret_userate){
        /*
        *para: server
        *return: -1: 不匹配; 0:匹配(AB都放); 1:匹配(A放); 2:匹配(B放);
        */
        int A_cpuCores=server.A_cpuCores,A_memorySize=server.A_memorySize;
        int B_cpuCores=server.B_cpuCores,B_memorySize=server.B_memorySize;
        int cpuCores=v.cpuCores, memorySize=v.memorySize, isDouble=v.isDouble;

        int m=-1;
        double userate=0.0;

        if(isDouble==0){
            //单节点部署
            if(A_cpuCores>=cpuCores&&A_memorySize>=memorySize){  //A节点满足
                double r=1-(A_cpuCores+A_memorySize-cpuCores-memorySize)/(double(server.cpuCores/2+server.memorySize/2));
                //r = r * (double(1)/(A_cpuCores+A_memorySize-cpuCores-memorySize));

                /*double cpu_r = 1-(A_cpuCores-cpuCores)/(double(server.cpuCores/2));
                double mem_r = 1-(A_memorySize-memorySize)/(double(server.memorySize/2));
                r = 0.75*cpu_r + 0.25*mem_r;*/
                if(userate<r){
                    m=1;userate=r;
                }
            }
            if(B_cpuCores>=cpuCores&&B_memorySize>=memorySize){ //B节点满足
                double r=1-(B_cpuCores+B_memorySize-cpuCores-memorySize)/(double(server.cpuCores/2+server.memorySize/2));
                //r = r * (double(1)/(B_cpuCores+B_memorySize-cpuCores-memorySize));

                /*double cpu_r = 1-(B_cpuCores-cpuCores)/(double(server.cpuCores/2));
                double mem_r = 1-(B_memorySize-memorySize)/(double(server.memorySize/2));
                r = 0.75*cpu_r + 0.25*mem_r;*/

                if(userate<r){
                    m=2;userate=r;
                }
            }
        }
        else{
            //双节点部署
            if(A_cpuCores>=cpuCores/2&&B_cpuCores>=cpuCores/2&&A_memorySize>=memorySize/2&&B_memorySize>=memorySize/2){
                m=0;userate=1-(A_cpuCores+B_cpuCores+A_memorySize+B_memorySize-cpuCores-memorySize)/(double(server.cpuCores+server.memorySize));
                //userate = userate * (double(1)/(A_cpuCores+B_cpuCores+A_memorySize+B_memorySize-cpuCores-memorySize));
                /*double cpu_r = 1-(A_cpuCores+B_cpuCores-cpuCores)/(double(server.cpuCores));
                double mem_r = 1-(A_memorySize+B_memorySize-memorySize)/(double(server.memorySize));
                userate = 0.75*cpu_r + 0.25*mem_r;*/
            }
        }

        ret_m=m;ret_userate=userate;
        return;
    }

    /*删除虚拟机*/
    void delVM(int vmid){
        if(vmid2loc.find(vmid)==vmid2loc.end()) cout<<"map error"<<endl;
        vector<int> loc =vmid2loc[vmid];
        int serverid=loc[0],m=loc[1];
        SERVER& server = PurServer[serverid];

        int vmtypeidx = vmid2vmtypeidx[vmid];
        VM& v = AllVm[vmtypeidx];
        int cpuCores=v.cpuCores, memorySize=v.memorySize;
        if(m==0){
            server.A_cpuCores=server.A_cpuCores+cpuCores/2;server.B_cpuCores=server.B_cpuCores+cpuCores/2;
            server.A_memorySize=server.A_memorySize+ memorySize/2;server.B_memorySize=server.B_memorySize+ memorySize/2;
        }
        else if(m==1){
            server.A_cpuCores=server.A_cpuCores+cpuCores;
            server.A_memorySize=server.A_memorySize+ memorySize;
        }
        else{
            server.B_cpuCores=server.B_cpuCores+cpuCores;
            server.B_memorySize=server.B_memorySize+ memorySize;
        }
        server.vmidset.erase(vmid);
        server.runvmcnt--;
    }

    /*将虚拟机放入指定的服务器*/
    void PutVM(VM& v,int serveridx, int m){
        /**
        * para: v:请求虚拟器, serveridx:服务器IDX, m:{0,1,2}
        * 将虚拟机按指定的方式放入指定的服务器
        **/
        int cpuCores=v.cpuCores, memorySize=v.memorySize, isDouble=v.isDouble, vmid=v.ID;
        SERVER& server=PurServer[serveridx];
        server.runvmcnt++;
        server.vmidset.insert(vmid);

        if(m==0) {
            server.A_cpuCores=server.A_cpuCores-cpuCores/2;server.B_cpuCores=server.B_cpuCores-cpuCores/2;
            server.A_memorySize=server.A_memorySize-memorySize/2;server.B_memorySize=server.B_memorySize-memorySize/2;
            vmid2loc[vmid]=vector<int>{serveridx,m};
        }
        if(m==1) {
            server.A_cpuCores=server.A_cpuCores - cpuCores;
            server.A_memorySize=server.A_memorySize- memorySize;
            vmid2loc[vmid]=vector<int>{serveridx,m};
        }
        if(m==2) {
            server.B_cpuCores=server.B_cpuCores - cpuCores;
            server.B_memorySize=server.B_memorySize - memorySize;
            vmid2loc[vmid]=vector<int>{serveridx,m};
        }
    }

    /*void PutVM(int vmid, int serveridx, int m){
        int vmtypeidx = vmid2vmtypeidx[vmid];
        VM v = AllVm[vmtypeidx];
        v.ID=vmid;
        PutVM(v,serveridx,m);
    }*/

    /*迁移虚拟机到指定服务器*/
    void MoveVM(int vmid,int SourceServerIdx,int TargertServerIdx,int m){
        int vmtypeidx = vmid2vmtypeidx[vmid];
        VM v = AllVm[vmtypeidx];
        v.ID=vmid;
        SERVER& server=PurServer[TargertServerIdx];

        delVM(vmid);                 //删除源服务器的虚拟机
        PutVM(v,TargertServerIdx,m); //转移虚拟机到目标服务器
        return;
    }

    int BuyServer(VM& v){
        /**
        * para: v: 请求的虚拟器
        *return: 购买服务器IDX
        **/

        int cpuCores=v.cpuCores, memorySize=v.memorySize, isDouble=v.isDouble;
        for(int i=0;i<serverNum;i++){
            SERVER& server = AllServer[i];
            if(isDouble==0){
                //单点部署
                if(server.A_cpuCores>=cpuCores&&server.A_memorySize>=memorySize)
                    return i;

            }
            else{
                // 双点部署
                if(server.A_cpuCores>=cpuCores/2&&server.A_memorySize>=memorySize/2)
                    return i;
            }
        }
        return -1;
    }

    int BuyServer(VM& v,int K){
        /**
        * para: v: 请求的虚拟器
        * return: 购买服务器IDX
        **/

        int cpuCores=v.cpuCores, memorySize=v.memorySize, isDouble=v.isDouble;
        vector<int> satify_servers;
        int min_cost=-1,MaxCostDet=50000;
        for(int i=0;i<serverNum;i++){
            SERVER& server = AllServer[i];
            if(isDouble==0){
                //单点部署
                if(server.A_cpuCores>=cpuCores&&server.A_memorySize>=memorySize){
                    int cost = server.serverCost+remain_days*server.powerCost;
                    if(min_cost==-1) min_cost=cost;
                    if(cost-min_cost>MaxCostDet) break;
                    satify_servers.push_back(i);
                }


            }
            else{
                // 双点部署
                if(server.A_cpuCores>=cpuCores/2&&server.A_memorySize>=memorySize/2){
                    int cost = server.serverCost+remain_days*server.powerCost;
                    if(min_cost==-1) min_cost=cost;
                    if(cost-min_cost>MaxCostDet) break;
                    satify_servers.push_back(i);
                }
            }

            if(satify_servers.size()>=K)
                break;
        }

        //选择CPU+MEM最大的服务器
        int max_capcity=0,max_serveridx=-1;
        for(int i=0;i<satify_servers.size();i++){
            SERVER& server = AllServer[satify_servers[i]];
            int capcity=4*server.cpuCores+server.memorySize;
            if(max_capcity<capcity){
                max_capcity=capcity;
                max_serveridx=satify_servers[i];
            }
        }

        /*for(int i=0;i<satify_servers.size();i++){
            SERVER& server = AllServer[satify_servers[i]];
            int cost = server.serverCost+remain_days*server.powerCost;
            printf("%d ",cost);
        }
        printf("\n");*/


        return max_serveridx;
    }

    int DealVMIDSeries(vector<int>& create_vmids, vector<int>& servertype2cnt){
        vector<VM> create_vms;

        for(int vmid:create_vmids){
            int vmtypeidx = vmid2vmtypeidx[vmid];
            VM v = AllVm[vmtypeidx]; v.ID=vmid;
            create_vms.push_back(v);
        }

        sort(create_vms.begin(),create_vms.end(),VMCMP);

        for(VM& v: create_vms){
            //首先挑选最佳服务器
            double max_use_rate=0.0;
            int flag_m=-1,flag_serveridx=-1;

            int cpuCores=v.cpuCores, memorySize=v.memorySize, isDouble=v.isDouble;
            int vmid=v.ID;

            for(int i=0;i<PurServerCnt;i++){
                SERVER& server = PurServer[i];
                int m=-1;
                double use_rate=-1;
                match(server, v, m, use_rate);
                if(m!=-1&&max_use_rate<use_rate){
                    max_use_rate=use_rate;
                    flag_m=m;
                    flag_serveridx=i;
                }
            }

            //如果当前购买的服务器都不能满足创建需求,购买符合条件的最便宜服务器
            if(flag_m==-1){
                int idx = BuyServer(v,greedy_k);
                if(PurServerCnt>=MAXSERVERCNT) return -1;
                flag_serveridx=PurServerCnt;
                servertype2cnt[idx]++;PurServer[PurServerCnt++] = AllServer[idx];
                SERVERCOST=SERVERCOST+AllServer[idx].serverCost;

                if(isDouble==0) flag_m=1;
                else            flag_m=0;
            }
            //将虚拟机转入指定的服务器
            PutVM(v,flag_serveridx,flag_m);
        }
        return 1;
    }

    /*每日的能耗成本*/
    int GetDayPower(){
        int cost=0;
        for(int i=0;i<PurServerCnt;i++){
            if(PurServer[i].runvmcnt>0)
                cost = cost + PurServer[i].powerCost;
        }
        return cost;
    }

    int HandleRequest(){
        clock_t start_time, end_time;
        start_time = clock();

        /**开始处理**/
        for(CurDay=0; CurDay<requestdays; CurDay++){
            remain_days = requestdays-CurDay;
            sort(AllServer.begin(),AllServer.end(),SERVERCMP);
            if(CurDay==0){
                //读入K天的数据
                IO::inputRequest(0,intervaldays);
            }
            else if(CurDay+intervaldays<=requestdays){
                //读入1天数据
                IO::inputRequest(CurDay+intervaldays-1,CurDay+intervaldays);
            }


            int PrevServerCnt=PurServerCnt;
            //printf("CurDay=%d  \n",CurDay);
            migrate();
            //printf("migrate cmplete\n");
            vector<int> servertype2cnt(serverNum,0);  // 购买服务器列表

            //执行序列

            int DayReqCnt=Request[CurDay].size();
            int p=0;
            vector<int> create_vmids; //连续的创建虚拟机ID序列

            while(p<DayReqCnt){
                vector<int>& req = Request[CurDay][p];
                int op=req[0],vmid=req[1];
                if (op==0){
                    // 遇到创建请求则将他放入当前虚拟机请求序列中
                    create_vmids.push_back(vmid);
                    p++;
                    CurVMCnt++;
                }
                else{
                    // 删除服务器
                    if(!create_vmids.empty()){
                        DealVMIDSeries(create_vmids, servertype2cnt);
                        create_vmids.clear();
                    }
                    delVM(vmid);
                    p++;
                    CurVMCnt--;
                }

                if(p==DayReqCnt&&!create_vmids.empty()){
                    DealVMIDSeries(create_vmids, servertype2cnt);
                    create_vmids.clear();
                }
            }


            expansion(servertype2cnt);

            /*给服务器重编号，后输出create操作序列*/
            int RemapID=PrevServerCnt;
            for(int i=0;i<servertype2cnt.size();i++){
                int cnt = servertype2cnt[i];
                if(cnt){
                    for(int j=PrevServerCnt;j<PurServerCnt;j++){
                        if(PurServer[j].serverType==AllServer[i].serverType)
                            PurServer[j].ID = RemapID++;
                    }
                }
            }

            for(vector<int>& req: Request[CurDay]){
                int op=req[0],vmid=req[1];
                if(op==0){
                    vector<int> loc =vmid2loc[vmid];
                    int serverid=loc[0],m=loc[1];
                    OutputCreateVM(PurServer[serverid].ID,m);
                }
            }

            /*计算当日电费*/
#ifdef TEST
            POWERCOST = POWERCOST + GetDayPower();
            TOTALCOST = SERVERCOST + POWERCOST;

            if((CurDay+1)%100==0){
                printf("Curday=%d Cost=%lld = %lld + %lld\n",CurDay+1,TOTALCOST,SERVERCOST,POWERCOST);
            }
#endif

#ifdef UPLOAD
            OutpytDay();
#endif // UPLOAD
        }


        //TOTALCOST = SERVERCOST + POWERCOST;

#ifdef TEST
        end_time = clock();
        printf("run time: %f s \n",double(end_time - start_time) / CLOCKS_PER_SEC);
        printf("server cost: %lld \npower cost: %lld \ntotal cost: %lld \n",SERVERCOST,POWERCOST,TOTALCOST);
#endif // TEST
        return 1;
    }

    /*******记录输入输出工具类函数******/
    void OutpytDay(){
        /**
        *function 输出当天的操作序列
        */
        for(string s:ExpanRes[CurDay])
            cout<<s;
        for(string s:MigrateRes[CurDay])
            cout<<s;
        for(string s:CreateVmRes[CurDay])
            cout<<s;
        fflush(stdout);
    }

    void OutputExpansion(vector<int>& servertype2cnt){
        /**
        *para: servertype2cnt,记录每种服务器买几个
        *function:生成购买序列，生成purchase输出序列
        */

        /*输出购买服务器种类数量*/
        int PurTypeCnt=0;   // 购买服务器种类的数量
        for(int i=0;i<serverNum;i++){
            if(servertype2cnt[i]) PurTypeCnt++;
        }
        string initBuy = "(purchase, ";
        initBuy += to_string(PurTypeCnt)+")\n";
        ExpanRes[CurDay].push_back(initBuy);

        /*输出购买序列*/
        for(int i=0;i<serverNum;i++){
            int cnt = servertype2cnt[i];
            if(cnt){
                string serverType = AllServer[i].serverType;
                string pauseInfo ="("+serverType+", ";
                pauseInfo+= std::to_string(cnt)+")\n";
                ExpanRes[CurDay].push_back(pauseInfo);
            }
        }
    }

    void OutputMigrate(unordered_map<int,vector<int>>& vmid2mvloc){
        int mvcnt=vmid2mvloc.size();
        string s = "(migration, "+to_string(mvcnt)+")\n";
        MigrateRes[CurDay].push_back(s);

        auto iter = vmid2mvloc.begin();
        for(;iter!=vmid2mvloc.end();iter++){
            int vmid=iter->first;
            vector<int>& loc=iter->second;
            int serveridx=loc[0],m=loc[1];

            string s;
            if(m==0){
                //s="("+to_string(vmid)+", "+to_string(serveridx)+")\n";
                s="("+to_string(vmid)+", "+to_string(PurServer[serveridx].ID)+")\n";
            }
            else if(m==1){
                //s="("+to_string(vmid)+", "+to_string(serveridx)+", A)\n";
                s="("+to_string(vmid)+", "+to_string(PurServer[serveridx].ID)+", A)\n";
            }
            else if(m==2){
                //s="("+to_string(vmid)+", "+to_string(serveridx)+", B)\n";
                s="("+to_string(vmid)+", "+to_string(PurServer[serveridx].ID)+", B)\n";
            }
            MigrateRes[CurDay].push_back(s);
        }
    }

    void OutputCreateVM(int serverid, int m){
        /**
        *para: serverid 服务器ID，m:存放方式，{0: A/B共同存放,1:存放在A,2:存放在B}
        *function: 输出每次的createVM序列
        */
        if(m==0) CreateVmRes[CurDay].push_back("("+to_string(serverid)+")\n");
        else if(m==1) CreateVmRes[CurDay].push_back("("+to_string(serverid)+", A)\n");
        else if(m==2) CreateVmRes[CurDay].push_back("("+to_string(serverid)+", B)\n");
    }

    /**************************/
};


int main() {
    IO::input();
    IO::inputRequestDay();

#ifdef TEST
    /*for(SERVER& ser:AllServer){
        cout<<ser.serverCost<<endl;
    }*/
#endif // TEST

    int failure_cnt=2000;
    Solver solver(1024,failure_cnt);
    int ret = solver.HandleRequest();
    if(ret==-1) {
#ifdef TEST
    printf("not satify\n");
#endif // TEST
    }
}
