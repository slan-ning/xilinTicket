
// BuyTicketDlg.cpp : 实现文件
//

#include "stdafx.h"
#include "BuyTicket.h"
#include "BuyTicketDlg.h"
#include "afxdialogex.h"
#include "Huoche.h"
#include "YzDlg.h"
#include <iostream>
#include <fstream>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// CBuyTicketDlg 对话框

CHuoche *huoche;

CBuyTicketDlg::CBuyTicketDlg(CWnd* pParent /*=NULL*/)
	: CDialogEx(CBuyTicketDlg::IDD, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CBuyTicketDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_LIST1, m_listbox);
}

BEGIN_MESSAGE_MAP(CBuyTicketDlg, CDialogEx)
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDC_BUTTON1, &CBuyTicketDlg::OnBnClickedButton1)
	ON_BN_CLICKED(IDC_BUTTON2, &CBuyTicketDlg::OnBnClickedButton2)
	ON_WM_TIMER()
	ON_BN_CLICKED(IDC_OPEN_URL, &CBuyTicketDlg::OnBnClickedOpenUrl)
	ON_STN_CLICKED(IDC_PIC, &CBuyTicketDlg::OnStnClickedPic)
	ON_BN_CLICKED(IDC_BLOG, &CBuyTicketDlg::OnBnClickedBlog)
END_MESSAGE_MAP()


// CBuyTicketDlg 消息处理程序

BOOL CBuyTicketDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// 设置此对话框的图标。当应用程序主窗口不是对话框时，框架将自动
	//  执行此操作
	SetIcon(m_hIcon, TRUE);			// 设置大图标
	SetIcon(m_hIcon, FALSE);		// 设置小图标

	huoche=new CHuoche(this);
	// TODO: 在此添加额外的初始化代码
	if (huoche->GetCode())
	{
		this->LoadYzCode();
	}

    CreateDirectory("c:\\12306",NULL);

	 std::ifstream userfile("c:\\buyticket.dat");
	 std::string  uname,upass,time,fromStation,toStation,date,name,id,phone,specialTrain;
	 std::string  name2,id2,phone2;
	 std::string  name3,id3,phone3;
        if(userfile.is_open())
        {
		std::getline(userfile,uname);
		std::getline(userfile,upass);
		std::getline(userfile,time);
		std::getline(userfile,fromStation);
		std::getline(userfile,toStation);
		std::getline(userfile,date);
		std::getline(userfile,name);
		std::getline(userfile,id);
		std::getline(userfile,phone);
		std::getline(userfile,specialTrain);

		std::getline(userfile,name2);
		std::getline(userfile,id2);
		std::getline(userfile,phone2);

		std::getline(userfile,name3);
		std::getline(userfile,id3);
		std::getline(userfile,phone3);
		userfile.close();
        this->GetDlgItem(IDC_EDIT_DOMAIN)->SetWindowText("kyfw.12306.cn");
		this->GetDlgItem(IDC_UNAME)->SetWindowText(uname.c_str());
		this->GetDlgItem(IDC_UPASS)->SetWindowText(upass.c_str());
		
		this->GetDlgItem(IDC_FROMCITY)->SetWindowText(fromStation.c_str());
		this->GetDlgItem(IDC_TOCITY)->SetWindowText(toStation.c_str());
		this->GetDlgItem(IDC_DATE)->SetWindowText(date.c_str());

		this->GetDlgItem(IDC_FULLNAME)->SetWindowText(name.c_str());
		this->GetDlgItem(IDC_IDCARD)->SetWindowText(id.c_str());
		this->GetDlgItem(IDC_PHONE)->SetWindowText(phone.c_str());
		this->GetDlgItem(IDC_TRAIN)->SetWindowText(specialTrain.c_str());

		this->GetDlgItem(IDC_FULLNAME2)->SetWindowText(name2.c_str());
		this->GetDlgItem(IDC_IDCARD2)->SetWindowText(id2.c_str());
		this->GetDlgItem(IDC_PHONE2)->SetWindowText(phone2.c_str());

		this->GetDlgItem(IDC_FULLNAME3)->SetWindowText(name3.c_str());
		this->GetDlgItem(IDC_IDCARD3)->SetWindowText(id3.c_str());
		this->GetDlgItem(IDC_PHONE3)->SetWindowText(phone3.c_str());
	}

	if(time=="") time="5";
	this->GetDlgItem(IDC_EDIT_TIME)->SetWindowText(time.c_str());

	
	
	
	((CButton*)GetDlgItem(IDC_CHECK_YW))->SetCheck(TRUE);
	((CButton*)GetDlgItem(IDC_CHECK_EDZ))->SetCheck(TRUE);

	

	return TRUE;  // 除非将焦点设置到控件，否则返回 TRUE
}

// 如果向对话框添加最小化按钮，则需要下面的代码
//  来绘制该图标。对于使用文档/视图模型的 MFC 应用程序，
//  这将由框架自动完成。

void CBuyTicketDlg::OnPaint()
{
	LoadYzCode();
	if (IsIconic())
	{
		CPaintDC dc(this); // 用于绘制的设备上下文

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// 使图标在工作区矩形中居中
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// 绘制图标
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

//当用户拖动最小化窗口时系统调用此函数取得光标
//显示。
HCURSOR CBuyTicketDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}



void CBuyTicketDlg::OnBnClickedButton1()
{
	CString code,name,pass;
	this->GetDlgItem(IDC_CODE)->GetWindowText(code);
	this->GetDlgItem(IDC_UNAME)->GetWindowText(name);
	this->GetDlgItem(IDC_UPASS)->GetWindowText(pass);
	std::string yzcode=code.GetBuffer();
	
	huoche->Login(name.GetBuffer(),pass.GetBuffer(),yzcode);
	
}


bool CBuyTicketDlg::LoadYzCode(void)
{
	CRect rect;
	this->GetDlgItem(IDC_PIC)->GetClientRect(&rect);     //m_picture为Picture Control控件变量，获得控件的区域对象

	CImage image;       //使用图片类
	image.Load("c:\\buyticket.png");   //装载路径下图片信息到图片类
    if(!image.IsNull()){
        CDC* pDC = this->GetDlgItem(IDC_PIC)->GetWindowDC();    //获得显示控件的DC
	    image.Draw( pDC -> m_hDC,rect);      //图片类的图片绘制Draw函数
	
	    ReleaseDC(pDC);
    }
	
	return true;
}





void CBuyTicketDlg::OnBnClickedButton2()
{
	CString time;
	this->GetDlgItem(IDC_EDIT_TIME)->GetWindowText(time);

	int ntime=atof(time)*1000;
	huoche->isInBuy=false;
	huoche->SerachTicketPage();
	this->KillTimer(1);
	this->SetTimer(1,ntime,NULL);

	std::ofstream userfile("c:\\buyticket.dat");
	CString uname,upass,fromStation,toStation,date,name,id,phone,specialTrain;
	CString name2,id2,phone2;
	CString name3,id3,phone3;
	this->GetDlgItem(IDC_UNAME)->GetWindowText(uname);
	this->GetDlgItem(IDC_UPASS)->GetWindowText(upass);
	this->GetDlgItem(IDC_FROMCITY)->GetWindowText(fromStation);
	this->GetDlgItem(IDC_TOCITY)->GetWindowText(toStation);
	this->GetDlgItem(IDC_DATE)->GetWindowText(date);
	this->GetDlgItem(IDC_FULLNAME)->GetWindowText(name);
	this->GetDlgItem(IDC_IDCARD)->GetWindowText(id);
	this->GetDlgItem(IDC_PHONE)->GetWindowText(phone);
	this->GetDlgItem(IDC_TRAIN)->GetWindowText(specialTrain);

	this->GetDlgItem(IDC_FULLNAME2)->GetWindowText(name2);
	this->GetDlgItem(IDC_IDCARD2)->GetWindowText(id2);
	this->GetDlgItem(IDC_PHONE2)->GetWindowText(phone2);

	this->GetDlgItem(IDC_FULLNAME3)->GetWindowText(name3);
	this->GetDlgItem(IDC_IDCARD3)->GetWindowText(id3);
	this->GetDlgItem(IDC_PHONE3)->GetWindowText(phone3);

	userfile<<uname.GetBuffer()<<"\n";
	userfile<<upass.GetBuffer()<<"\n";
	userfile<<time.GetBuffer()<<"\n";
	userfile<<fromStation.GetBuffer()<<"\n";
	userfile<<toStation.GetBuffer()<<"\n";
	userfile<<date.GetBuffer()<<"\n";
	userfile<<name.GetBuffer()<<"\n";
	userfile<<id.GetBuffer()<<"\n";
	userfile<<phone.GetBuffer()<<"\n";
	userfile<<specialTrain.GetBuffer()<<"\n";

	userfile<<name2.GetBuffer()<<"\n";
	userfile<<id2.GetBuffer()<<"\n";
	userfile<<phone2.GetBuffer()<<"\n";

	userfile<<name3.GetBuffer()<<"\n";
	userfile<<id3.GetBuffer()<<"\n";
	userfile<<phone3.GetBuffer()<<"\n";

        userfile.close();
	
}


void CBuyTicketDlg::OnTimer(UINT_PTR nIDEvent)
{
	// TODO: 在此添加消息处理程序代码和/或调用默认值
	CString fromcity,tocity,date,train;
	this->GetDlgItem(IDC_FROMCITY)->GetWindowText(fromcity);
	this->GetDlgItem(IDC_TOCITY)->GetWindowText(tocity);
	this->GetDlgItem(IDC_DATE)->GetWindowText(date);
	this->GetDlgItem(IDC_TRAIN)->GetWindowText(train);

	if(!huoche->isInBuy)
	{
		huoche->train=train.GetBuffer();
		huoche->SearchTicket(fromcity.GetBuffer(),tocity.GetBuffer(),date.GetBuffer());
	}

	CDialogEx::OnTimer(nIDEvent);
}


void CBuyTicketDlg::OnBnClickedOpenUrl()
{
	ShellExecute(NULL, _T("open"), _T("iexplore"), _T("https://kyfw.12306.cn/otn/index/initMy12306"), NULL, SW_SHOWNORMAL);
}


void CBuyTicketDlg::OnStnClickedPic()
{
	huoche->GetCode();
	this->LoadYzCode();
}


void CBuyTicketDlg::OnBnClickedBlog()
{
	ShellExecute(NULL, _T("open"), _T("http://www.xiaoqin.in/index.php?a=details&aid=110"),NULL, NULL, SW_SHOWNORMAL);
}


void CBuyTicketDlg::OnOK()
{
	// 屏蔽回车关闭

	//CDialogEx::OnOK();
}


void CBuyTicketDlg::OnCancel()
{

	CDialogEx::OnCancel();
}


BOOL CBuyTicketDlg::PreTranslateMessage(MSG* pMsg)
{
	if(pMsg->message == WM_KEYDOWN) 
	{
		switch(pMsg->wParam) 
		{

		case VK_ESCAPE: //ESC
			return TRUE;
		}
	}

	return CDialogEx::PreTranslateMessage(pMsg);
}
