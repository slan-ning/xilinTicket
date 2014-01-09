// YzDlg.cpp : 实现文件
//

#include "stdafx.h"
#include "BuyTicket.h"
#include "YzDlg.h"
#include "afxdialogex.h"
#include "Huoche.h"


// CYzDlg 对话框

IMPLEMENT_DYNAMIC(CYzDlg, CDialogEx)

CYzDlg::CYzDlg(CWnd* pParent /*=NULL*/)
	: CDialogEx(CYzDlg::IDD, pParent)
{

}

CYzDlg::~CYzDlg()
{
}

void CYzDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}


BEGIN_MESSAGE_MAP(CYzDlg, CDialogEx)
	ON_BN_CLICKED(IDC_BUTTON1, &CYzDlg::OnBnClickedButton1)
	ON_WM_PAINT()
	ON_STN_CLICKED(IDC_PIC2, &CYzDlg::OnStnClickedPic2)
END_MESSAGE_MAP()


// CYzDlg 消息处理程序


void CYzDlg::OnBnClickedButton1()
{
	CString code;
	this->GetDlgItem(IDC_EDIT_IMG2)->GetWindowText(code);
	this->yzcode=code.GetBuffer();

	if(this->yzcode=="")
	{
		this->MessageBox("输入不能为空!");
	}
	else
	{
		this->OnOK();
	}
}


BOOL CYzDlg::OnInitDialog()
{

	return CDialogEx::OnInitDialog();
}


void CYzDlg::OnPaint()
{
	loadImg();
	
	CDialogEx::OnPaint();
}


void CYzDlg::OnStnClickedPic2()
{
    this->file_path=huoche->loadCode2();
	this->loadImg();
}


void CYzDlg::loadImg(void)
{
	CRect rect;
	this->GetDlgItem(IDC_PIC2)->GetClientRect(&rect);     //m_picture为Picture Control控件变量，获得控件的区域对象

	CImage image;       //使用图片类
	image.Load(this->file_path.c_str());   //装载路径下图片信息到图片类
    if(!image.IsNull()){

        CDC* pDC = this->GetDlgItem(IDC_PIC2)->GetWindowDC();    //获得显示控件的DC
	    image.Draw( pDC -> m_hDC,rect);      //图片类的图片绘制Draw函数
	
	    ReleaseDC(pDC);
    }
	
}


BOOL CYzDlg::PreTranslateMessage(MSG* pMsg)
{
	if(pMsg -> message == WM_KEYDOWN)
	{
		CEdit* pEdit = (CEdit*)GetDlgItem(IDC_EDIT_IMG2);
        ASSERT(pEdit);
        if(pMsg->hwnd == pEdit->GetSafeHwnd() && VK_RETURN == pMsg->wParam)
        {
            this->OnBnClickedButton1();
            return TRUE;
        }
	}	

	return CDialogEx::PreTranslateMessage(pMsg);
}
