﻿/**
 * @file CMediumServer.cpp
 * @author DennisMi (https://www.dennisthink.com/)
 * @brief 业务逻辑的主流程类
 * @version 0.1
 * @date 2019-07-20
 * 
 * @copyright Copyright (c) 2019
 * 
 */

#include "CMediumServer.h"
#include "json11.hpp"
#include "DaemonSvcApp.h"
#include "EncodingUtil.h"
#include "CTimeUtil.h"
#include "md5.h"
const std::string DEFAULT_TEAM_ID = "10000000";
const std::string DEFAULT_TEAM_NAME = EncodeUtil::AnsiToUtf8("我的好友");
namespace ChatServer
{
std::shared_ptr<spdlog::logger> CChatServer::ms_loger;

/**
 * @brief 加载配置文件
 * 
 * @param cfg 配置文件的json
 * @param ec 发生的异常
 */
void CChatServer::loadConfig(const json11::Json &cfg, std::error_code& ec) 
{
	auto serverCfg = cfg["server"];
	LOG_INFO(ms_loger,"loadConfig [{} {}]", __FILENAME__, __LINE__);
    ec.clear();
    m_serverCfg.m_strServerIp=serverCfg["ip"].string_value();
    m_serverCfg.m_nPort=(uint16_t)serverCfg["port"].int_value();
    LOG_INFO(ms_loger,"ServerIp: {} [{} {}]",m_serverCfg.to_string(), __FILENAME__, __LINE__);
    if(!m_serverCfg.Valid())
	{
		LOG_ERR(ms_loger,"Config Error {}  [{} {}]",m_serverCfg.to_string(),__FILENAME__, __LINE__);
        return;
    }

	auto clientsCfg = cfg["clients"];
	if(!clientsCfg.is_array())
	{
		LOG_ERR(ms_loger,"Clients Config Error {} [{} {}]",cfg.string_value(),__FILENAME__, __LINE__);
        return;
	}
	for(auto item:clientsCfg.array_items())
	{
		IpPortCfg clientCfg;
		clientCfg.m_strServerIp = item["ip"].string_value();
		clientCfg.m_nPort = item["port"].int_value();
		LOG_INFO(ms_loger,"Client Config: {}  [{} {}]",clientCfg.to_string(), __FILENAME__, __LINE__);
		m_clientCfgVec.push_back(clientCfg);
	}
}

/**
 * @brief 处理心跳请求消息
 * 
 * @param pSess 
 * @param reqMsg 
 */
void CChatServer::HandleUserKeepAliveReq(const std::shared_ptr<CServerSess> pSess, const KeepAliveReqMsg& reqMsg)
{
	if (pSess)
	{
		KeepAliveRspMsg rspMsg;
		rspMsg.m_strClientId = reqMsg.m_strClientId;
		pSess->SendMsg(&rspMsg);
	}
}

/**
 * @brief 处理心跳回复消息
 * 
 * @param pSess 
 * @param rspMsg 
 */
void CChatServer::HandleUserKeepAliveRsp(const std::shared_ptr<CServerSess> pSess, const KeepAliveRspMsg& rspMsg)
{
	if (pSess)
	{

	}
}
/**
 * @brief 检查所有的socket连接
 * 
 */
void CChatServer::CheckAllConnect()
{
	for (auto item : m_UserSessVec) {
		OnAddFriendNotifyReqMsg(item.first);
		OnAddFriendRecvReqMsg(item.first);
		//OnUserReceiveMsg(item.first);
	}
}

/**
 * @brief 接受来自客户端的连接
 * 
 */
void CChatServer::do_accept() 
{
	LOG_INFO(ms_loger,"CMediumServer do_accept[{} {}]", __FILENAME__, __LINE__);
	m_acceptor.async_accept(m_socket, [this](std::error_code ec) {
		if (!ec)
		{
			LOG_INFO(ms_loger,"Server accept Successed [{} {}]", __FILENAME__, __LINE__);
			auto agentSess= std::make_shared<CServerSess>(std::move(m_socket),this);
			agentSess->Start();
			m_listenList.push_back(agentSess);
		}
		do_accept();
	});
}

/**
 * @brief 响应定时器的任务，一些需要定时处理的任务，可以写成一个函数，在此函数中调用
 * 
 */
void CChatServer::OnTimer()
{
	LOG_INFO(this->ms_loger,"{} Users Is OnLine Chat Server [ {} {} ]",m_UserSessVec.size(), __FILENAME__,__LINE__);
	for(auto& item:m_queueVec)
	{
		item->OnTimer();
	}
	CheckAllConnect();
}

/**
 * @brief 设置定时器，单位秒
 * 
 * @param nSeconds 定时的秒数
 */
void CChatServer::SetTimer(int nSeconds)
{
	if(m_timer)
	{
		m_timer->expires_from_now(std::chrono::seconds(nSeconds));
		auto self = shared_from_this();
		m_timer->async_wait([this,nSeconds,self](const std::error_code& ec){
			if(!ec)
			{
				this->OnTimer();
				this->SetTimer(nSeconds);
			}
			else
			{
				LOG_WARN(this->ms_loger,"On Timer at ChatServer {}  [{} {}]",ec.message(),__FILENAME__,__LINE__);
			}
		});
	}
}

void CChatServer::Handle_RecvTcpMsg(const std::shared_ptr<CServerSess> pSess, const TransBaseMsg_t* pMsg)
{
	LOG_INFO(ms_loger, "[ {} ] RECV: [ {} {} ]  [ {} {} ]", pSess->UserId(), MsgType(pMsg->GetType()), pMsg->to_string(), __FILENAME__, __LINE__);
	switch (pMsg->GetType())
	{
	case E_MsgType::KeepAliveReq_Type:
	{
		KeepAliveReqMsg msg;
		if (msg.FromString(pMsg->to_string()))
		{
			HandleUserKeepAliveReq(pSess, msg);
		}
	}
	break;
	case E_MsgType::KeepAliveRsp_Type:
	{
		KeepAliveRspMsg rspMsg;
		if (rspMsg.FromString(pMsg->to_string()))
		{
			HandleUserKeepAliveRsp(pSess, rspMsg);
		}
	}
	break;
	case E_MsgType::UserLoginReq_Type:
	{
		UserLoginReqMsg msg;
		if (msg.FromString(pMsg->to_string())) {
			HandleUserLoginReq(pSess, msg);
		}
		else {

		}
	}break;
	case E_MsgType::UserLogoutReq_Type: {
		UserLogoutReqMsg msg;
		if (msg.FromString(pMsg->to_string())) {
			HandleUserLogoutReq(pSess, msg);
		}
	}break;
	case E_MsgType::UserRegisterReq_Type:
	{
		UserRegisterReqMsg msg;
		if (msg.FromString(pMsg->to_string())) {
			HandleUserRegisterReq(pSess, msg);
		}
	}break;
	case E_MsgType::UserUnRegisterReq_Type:
	{
		UserUnRegisterReqMsg msg;
		if (msg.FromString(pMsg->to_string())) {
			HandleUserUnRegisterReq(pSess, msg);
		}
	}break;
	case E_MsgType::FriendChatSendTxtMsgReq_Type:
	{
		FriendChatSendTxtReqMsg msg;
		if (msg.FromString(pMsg->to_string())) {
			HandleFriendChatSendTxtReq(pSess, msg);
		}
	}break;

	case E_MsgType::FriendChatReceiveTxtMsgRsp_Type:
	{
		FriendChatRecvTxtRspMsg msg;
		if (msg.FromString(pMsg->to_string())) {
			HandleFriendChatRecvMsgRsp(pSess, msg);
		}
	}break;
	case E_MsgType::GetFriendListReq_Type:
	{
		GetFriendListReqMsg msg;
		if (msg.FromString(pMsg->to_string())) {
			HandleGetFriendListReq(pSess, msg);
		}
	}break;
	case E_MsgType::FindFriendReq_Type:
	{
		FindFriendReqMsg msg;
		if (msg.FromString(pMsg->to_string())) {
			HandleFindFriendReq(pSess, msg);
		}
	}break;
	case E_MsgType::AddFriendSendReq_Type:
	{
		AddFriendSendReqMsg msg;
		if (msg.FromString(pMsg->to_string())) {
			HandleAddFriendReq(pSess, msg);
		}
	}break;
	case E_MsgType::AddFriendRecvRsp_Type:
	{
		AddFriendRecvRspMsg msg;
		if (msg.FromString(pMsg->to_string())) {
			HandleAddFriendRecvRsp(pSess, msg);
		}
	}break;
	case E_MsgType::AddFriendNotifyRsp_Type:
	{
		AddFriendNotifyRspMsg msg;
		if (msg.FromString(pMsg->to_string())) {
			HandleAddFriendNotifyRsp(pSess, msg);
		}
	}break;
	case E_MsgType::RemoveFriendReq_Type:
	{
		RemoveFriendReqMsg msg;
		if (msg.FromString(pMsg->to_string())) {
			HandleRemoveFriendReq(pSess, msg);
		}
	}break;
	case E_MsgType::AddTeamReq_Type:
	{
		AddTeamReqMsg msg;
		if (msg.FromString(pMsg->to_string())) {
			HandleAddTeamReq(pSess, msg);
		}
	}break;
	case E_MsgType::RemoveTeamReq_Type:
	{
		RemoveTeamReqMsg msg;
		if (msg.FromString(pMsg->to_string())) {
			HandleRemoveTeamReq(pSess, msg);
		}
	}break;
	case E_MsgType::MoveFriendToTeamReq_Type: {
		MoveFriendToTeamReqMsg msg;
		if (msg.FromString(pMsg->to_string())) {
			HandleMoveFriendToTeamReq(pSess, msg);
		}
	}break;
	case E_MsgType::CreateGroupReq_Type:
	{
		CreateGroupReqMsg msg;
		if (msg.FromString(pMsg->to_string())) {
			HandleCreateGroupReq(pSess, msg);
		}
	}break;
	case E_MsgType::DestroyGroupReq_Type:
	{
		DestroyGroupReqMsg msg;
		if (msg.FromString(pMsg->to_string())) {
			HandleDestroyGroupReq(pSess, msg);
		}
	}break;
	case E_MsgType::FindGroupReq_Type:
	{
		FindGroupReqMsg msg;
		if (msg.FromString(pMsg->to_string())) {
			HandleFindGroupReq(pSess, msg);
		}
	}break;
	case E_MsgType::GetGroupListReq_Type:
	{
		GetGroupListReqMsg msg;
		if (msg.FromString(pMsg->to_string())) {
			HandleGetGroupListReq(pSess, msg);
		}
	}break;
	case E_MsgType::SendGroupTextMsgReq_Type:
	{
		SendGroupTextMsgReqMsg msg;
		if (msg.FromString(pMsg->to_string())) {
			HandleSendGroupTextReq(pSess, msg);
		}
	}break;
	case E_MsgType::AddToGroupReq_Type: {
		AddToGroupReqMsg msg;
		if (msg.FromString(pMsg->to_string())) {
			HandleAddToGroupReq(pSess, msg);
		}
	}break;
	case E_MsgType::QuitGroupReq_Type:
	{
		QuitFromGroupReqMsg reqMsg;
		if (reqMsg.FromString(pMsg->to_string())) {
			HandleQuitGroupReqMsg(pSess, reqMsg);
		}
	}break;
	case E_MsgType::RecvGroupTextMsgRsp_Type:
	{
		RecvGroupTextMsgRspMsg msg;
		if (msg.FromString(pMsg->to_string())) {
			HandleRecvGroupTextMsgRspMsg(pSess, msg);
		}
	}break;
	case E_MsgType::FriendSendFileMsgReq_Type:
	{
		FriendSendFileMsgReqMsg reqMsg;
		if (reqMsg.FromString(pMsg->to_string())) {
			HandleFriendSendFileReq(pSess, reqMsg);
		}
	}break;
	case E_MsgType::FriendRecvFileMsgRsp_Type:
	{
		FriendRecvFileMsgRspMsg reqMsg;
		if (reqMsg.FromString(pMsg->to_string())) {
			HandleFriendRecvFileRsp(pSess, reqMsg);
		}
	}break;
	case E_MsgType::FriendNotifyFileMsgRsp_Type:
	{
		FriendNotifyFileMsgRspMsg rspMsg;
		if (rspMsg.FromString(pMsg->to_string())) {
			HandleFriendNotifyFileRsp(pSess, rspMsg);
		}
	}break;
	case E_MsgType::FileVerifyReq_Type:
	{
		FileVerifyReqMsg reqMsg;
		if (reqMsg.FromString(pMsg->to_string())) {
			HandleFileVerifyReq(pSess, reqMsg);
		}
	}break;
	case E_MsgType::FileVerifyRsp_Type:
	{
		FileVerifyRspMsg rspMsg;
		if (rspMsg.FromString(pMsg->to_string())) {
			HandleFileVerifyRsp(pSess, rspMsg);
		}
	}break;
	case E_MsgType::UserKickOffRsp_Type:
	{
		UserKickOffRspMsg rspMsg;
		if (rspMsg.FromString(pMsg->to_string())) {
			HandleUserKickOffRsp(pSess, rspMsg);
		}
	}break;
	case E_MsgType::FriendUnReadMsgNotifyRsp_Type:
	{
		FriendUnReadNotifyRspMsg rspMsg;
		if (rspMsg.FromString(pMsg->to_string())) {
			HandleFriendUnReadNotifyRspMsg(pSess, rspMsg);
		}
	}break;
	case E_MsgType::UpdateFriendListNotifyRsp_Type:
	{
		UpdateFriendListNotifyRspMsg rspMsg;
		if (rspMsg.FromString(pMsg->to_string())) {
			HandleUpdateFriendListRsp(pSess, rspMsg);
		}
	}break;
	case E_MsgType::UpdateGroupListNotifyRsp_Type:
	{
		UpdateGroupListNotifyRspMsg rspMsg;
		if (rspMsg.FromString(pMsg->to_string())) {
			HandleUpdateGroupListRsp(pSess, rspMsg);
		}
	}break;
	case E_MsgType::QueryUserUdpAddrReq_Type:
	{
		QueryUserUdpAddrReqMsg reqMsg;
		if (reqMsg.FromString(pMsg->to_string())) {

		}

	}break;
	case E_MsgType::FileSendDataBeginReq_Type:
	{
		FileSendDataBeginReq reqMsg;
		if (reqMsg.FromString(pMsg->to_string())) {
			HandleFileSendDataBeginReq(pSess, reqMsg);
		}
	}break;
	case E_MsgType::FileSendDataBeginRsp_Type:
	{
		FileSendDataBeginRsp rspMsg;
		if (rspMsg.FromString(pMsg->to_string())) {
			HandleFileSendDataBeginRsp(pSess, rspMsg);
		}
	}break;
	case E_MsgType::FileDownLoadReq_Type:
	{
		FileDownLoadReqMsg reqMsg;
		if (reqMsg.FromString(pMsg->to_string())) {
			HandleFileDownLoadReq(pSess, reqMsg);
		}
	}break;
	default:
	{
		LOG_ERR(ms_loger, "User:{} Unhandle E_MsgType:{} [ {} {} ]", pSess->UserId(), MsgType(pMsg->GetType()), __FILENAME__, __LINE__);
	}break;
	}
}
/**
 * @brief 处理接收的UDP消息
 * 在此函数完成UDP消息的分发
 * @param sendPt UDP消息的发送地址
 * @param pMsg 
 */
void CChatServer::HandleRecvUdpMsg(const asio::ip::udp::endpoint sendPt, const TransBaseMsg_t* pMsg)
{
	if (pMsg)
	{
		switch (pMsg->GetType())
		{
		case E_MsgType::KeepAliveReq_Type:
		{
			KeepAliveReqMsg reqMsg;
			if (reqMsg.FromString(pMsg->to_string()))
			{
				KeepAliveRspMsg rspMsg;
				rspMsg.m_strClientId = reqMsg.m_strClientId;
				m_udpServer->sendMsg(sendPt, &rspMsg);
				auto item = m_UserSessVec.find(reqMsg.m_strClientId);
				if (item != m_UserSessVec.end())
				{
					auto udpItem = m_userIdUdpAddrMap.find(reqMsg.m_strClientId);
					if(udpItem != m_userIdUdpAddrMap.end())
					{
						IpPortCfg udpIp;
						udpIp.m_strServerIp = sendPt.address().to_string();
						udpIp.m_nPort = sendPt.port();
						if (udpItem->second.m_strServerIp == udpIp.m_strServerIp && 
							udpItem->second.m_nPort == udpIp.m_nPort )
						{

						}
						else
						{
							m_userIdUdpAddrMap.erase(reqMsg.m_strClientId);
							m_userIdUdpAddrMap.insert({ reqMsg.m_strClientId,udpIp });
						}
					}
					else
					{
						IpPortCfg udpIp;
						udpIp.m_strServerIp = sendPt.address().to_string();
						udpIp.m_nPort = sendPt.port();
						m_userIdUdpAddrMap.insert({ reqMsg.m_strClientId,udpIp });
					}
				}
			}
			else
			{

			}
		}break;
		case E_MsgType::FileSendDataReq_Type:
		{
			FileDataSendReqMsg reqMsg;
			if (reqMsg.FromString(pMsg->to_string())) {
				Handle_UdpFileDataSendReqMsg(sendPt,reqMsg);
			}
		}break;
		case E_MsgType::FileRecvDataRsp_Type:
		{
			FileDataRecvRspMsg rspMsg;
			if (rspMsg.FromString(pMsg->to_string())) {
				Handle_RecvUdpMsg(sendPt, rspMsg);
			}
		}break;
		default:
		{

		}break;
		}
	}
}

/**
 * @brief 获取Listen的服务器的IP和端口
 * 
 * @return std::string 
 */
std::string CChatServer::getServerIpPort()
{
	return m_serverCfg.to_string();
}

/**
 * @brief 处理从ClientSess中，回传的消息
 * 
 * @param msg 消息
 */
void CChatServer::SendBack(const TransBaseMsg_t *msg)
{
	if(msg)
	{
		LOG_INFO(ms_loger,"{} [{} {}]",msg->to_string(), __FILENAME__, __LINE__);
	}
}

FriendChatMsg_s DbBeanToMsgBean(const T_USER_CHAT_MSG& msgBean)
{
	FriendChatMsg_s result;
	
	result.m_strChatMsgId = msgBean.m_strF_MSG_ID;
	result.m_strSenderId = msgBean.m_strF_FROM_ID;
	result.m_strReceiverId = msgBean.m_strF_TO_ID;
	result.m_strContext = msgBean.m_strF_MSG_CONTEXT;
	result.m_strMsgTime = msgBean.m_strF_CREATE_TIME;
	result.m_fontInfo.FromString(msgBean.m_strF_OTHER_INFO);

	return result;
}
/**
 * @brief 响应用户收到消息，在用户登陆以后或者有人给该用户发送消息的时候调用
 * 
 * @param strUser 接收消息的用户名
 * @return true 接收成功
 * @return false 接收失败
 */
bool CChatServer::OnUserReceiveMsg(const std::string strUserId) {
	T_USER_CHAT_MSG msgBean;
	if (m_util.SelectUnReadFriendChatMsg(strUserId, msgBean))
	{
		if(CHAT_MSG_TYPE::E_CHAT_TEXT_TYPE == msgBean.m_eChatMsgType)
		{
			FriendChatRecvTxtReqMsg reqMsg;
			reqMsg.m_strMsgId = CreateMsgId();
			reqMsg.m_chatMsg = DbBeanToMsgBean(msgBean);
			{
				auto item = m_UserSessVec.find(strUserId);
				if (item != m_UserSessVec.end() && item->second->IsConnected())
				{
					item->second->SendMsg(&reqMsg);
				}
			}
		}
		return true;
	}
	else {
		m_clientStateMap[strUserId] = CLIENT_SESS_STATE::SESS_IDLE_STATE;
		return false;
	}
}


/**
 * @brief 处理添加好友的回复请求消息，在用户登录的时候
 * 
 * @param strUser 用户名
 * @return true 成功
 * @return false 失败
 */
bool CChatServer::OnAddFriendRecvReqMsg(const std::string strUser)
{
	T_ADD_FRIEND_MSG_BEAN msgBean;
	if (m_util.SelectUnReadAddFriendMsg(strUser, msgBean))
	{
		{
			AddFriendRecvReqMsg reqMsg;

			reqMsg.m_strMsgId = msgBean.m_strF_MSG_ID;
			reqMsg.m_strFriendId = msgBean.m_strF_FRIEND_ID;
			reqMsg.m_strUserId = strUser;

			{
				auto item = m_UserSessVec.find(strUser);
				if (item != m_UserSessVec.end() && item->second->IsConnected())
				{
					item->second->SendMsg(&reqMsg);
				}
				else
				{
					LOG_INFO(ms_loger, "Could Not Find Sess For {} [{}  {}]", strUser, __FILENAME__, __LINE__);
				}
			}

		}
		return true;
	}
	else {
		LOG_INFO(ms_loger, "{} has No UnRead Add Friend Msg [{} {}]",strUser, __FILENAME__, __LINE__);
		return false;
	}
}


/**
 * @brief 响应用户状态检查
 * 
 * @param strUserId 用户ID
 */
void CChatServer::OnUserStateCheck(const std::string strUserId)
{
	auto stateItem = m_clientStateMap.find(strUserId);
	auto item = m_userIdUdpAddrMap.find(strUserId);
	if (stateItem != m_clientStateMap.end() && 
		item != m_userIdUdpAddrMap.end())
	{
		T_USER_CHAT_MSG chatMsg;
		if (stateItem->second == CLIENT_SESS_STATE::SESS_IDLE_STATE) {
			if (m_util.SelectUnReadFriendChatMsg(strUserId, chatMsg)) {
				m_clientStateMap[strUserId] = CLIENT_SESS_STATE::SESS_FRIEND_MSG_SEND_RECV_STATE;
				FriendUnReadNotifyReqMsg reqMsg;
				reqMsg.m_strMsgId = std::to_string(m_MsgID_Util.nextId());
				reqMsg.m_strUserId = strUserId;

				auto item = m_UserSessVec.find(strUserId);
				if (item != m_UserSessVec.end()) {
					item->second->SendMsg(&reqMsg);
				}
				else
				{
					LOG_ERR(ms_loger, "User{} Has No Sess [{} {}]", strUserId, __FILENAME__, __LINE__);
				}
			}
			else
			{
				LOG_INFO(ms_loger, "User {} Is Recv Msg [{} {}]", strUserId,__FILENAME__, __LINE__);
			}
		}
		else
		{

		}
	}
	else
	{
		LOG_WARN(ms_loger, "No User {} State [ {}  {} ]", strUserId, __FILENAME__, __LINE__);
	}
}

/**
 * @brief 处理用户的登陆请求
 * 
 * @param pSess 用户的连接会话
 * @param reqMsg 登陆请求消息
 */
void CChatServer::HandleUserLoginReq(const std::shared_ptr<CServerSess>& pSess, const UserLoginReqMsg& reqMsg)
{
	UserLoginRspMsg rspMsg = DoUserLoginReq(reqMsg);
	rspMsg.m_strMsgId = reqMsg.m_strMsgId;
	if (pSess)
	{
		pSess->SendMsg(&rspMsg);
	}

	if (rspMsg.m_eErrCode == ERROR_CODE_TYPE::E_CODE_SUCCEED)
	{
		HandleReLogin(rspMsg.m_strUserId, pSess);
		//OnAddFriendRecvReqMsg(rspMsg.m_strUserId);
		//OnAddFriendNotifyReqMsg(rspMsg.m_strUserId);
		NotifyUserFriends(rspMsg.m_strUserId);
	}
}

/**
 * @brief 处理用户未读消息的回复
 * 
 * @param pSess 
 * @param rspMsg 
 */
void CChatServer::HandleFriendUnReadNotifyRspMsg(const std::shared_ptr<CServerSess>& pSess, const FriendUnReadNotifyRspMsg& rspMsg)
{
	m_clientStateMap[rspMsg.m_strUserId] = CLIENT_SESS_STATE::SESS_FRIEND_MSG_SEND_RECV_STATE;
	OnUserReceiveMsg(rspMsg.m_strUserId);
}

/**
 * @brief 处理用户的KeepAlive的请求
 * 
 * @param pSess 用户会话
 */
void CChatServer::HandleUserKeepAliveReq(const std::shared_ptr<CServerSess> pSess)
{
	if ( (pSess) &&
		(!pSess->UserId().empty()))
	{
		OnUserStateCheck(pSess->UserId());
		//OnAddFriendRecvReqMsg(pSess->UserId());
		//OnAddFriendNotifyReqMsg(pSess->UserId());
	}
}
/**
 * @brief 处理用户登出请求
 * 
 * @param pSess 用户连接会话
 * @param reqMsg 登出请求消息
 */
void CChatServer::HandleUserLogoutReq(const std::shared_ptr<CServerSess>& pSess, const UserLogoutReqMsg& reqMsg)
{
	UserLogoutRspMsg rspMsg = DoUserLogoutReq(reqMsg);
	rspMsg.m_strMsgId = reqMsg.m_strMsgId;
	if (pSess)
	{
		pSess->SendMsg(&rspMsg);
		NotifyUserFriends(pSess->UserId());
		OnSessClose(pSess);
	}

}

/**
 * @brief 处理用户注册请求
 * 
 * @param pSess 用户连接会话
 * @param reqMsg 注册请求消息
 */
void CChatServer::HandleUserRegisterReq(const std::shared_ptr<CServerSess>& pSess, const UserRegisterReqMsg& reqMsg)
{
	UserRegisterRspMsg rspMsg = DoUserRegisterReq(reqMsg);
	rspMsg.m_strMsgId = reqMsg.m_strMsgId;
	if (pSess)
	{
		pSess->SendMsg(&rspMsg);
	}
}

/**
 * @brief 处理用户注销请求
 * 
 * @param pSess 用户连接会话
 * @param reqMsg 注销请求
 */
void CChatServer::HandleUserUnRegisterReq(const std::shared_ptr<CServerSess>& pSess, const UserUnRegisterReqMsg& reqMsg)
{
	UserUnRegisterRspMsg rspMsg = DoUserUnRegisterReq(reqMsg);
	rspMsg.m_strMsgId = reqMsg.m_strMsgId;
	if (pSess)
	{
		pSess->SendMsg(&rspMsg);
	}
}



void CChatServer::HandleFileSendDataBeginReq(const std::shared_ptr<CServerSess>& pSess, const FileSendDataBeginReq& req)
{
	{
		std::string strFolder = m_fileUtil.GetCurDir() + req.m_strUserId + "\\";
		if (!m_fileUtil.IsFolder(strFolder)) {
			m_fileUtil.CreateFolder(strFolder);
		}

		T_FILE_HASH_BEAN bean;
		if(m_util.SelectFileByHash(bean,req.m_strFileHash))
		{
			std::string strFileName = GetFilePathByUserIdAndFileName(bean.m_strF_USER_ID, bean.m_strF_FILE_NAME);
			if (m_fileUtil.IsFileExist(strFileName))
			{
				FileSendDataBeginRsp rspMsg;
				rspMsg.m_errCode = ERROR_CODE_TYPE::E_CODE_FILE_HAS_EXIST;
				rspMsg.m_nFileId = req.m_nFileId;
				rspMsg.m_strFileName = req.m_strFileName;
				rspMsg.m_strFriendId = req.m_strFriendId;
				rspMsg.m_strUserId = req.m_strUserId;
				rspMsg.m_strMsgId = req.m_strMsgId;
				pSess->SendMsg(&rspMsg);
				return;
			}
			else
			{
				m_util.DeleteFileByHash(req.m_strFileHash);
			}
		}
		std::string strFileName = strFolder + req.m_strFileName;
		if (!m_fileUtil.IsFileExist(strFileName)) {
			m_fileUtil.OpenWriteFile(req.m_nFileId, strFileName);
		}
	}

	FileSendDataBeginRsp rspMsg;
	rspMsg.m_errCode = ERROR_CODE_TYPE::E_CODE_SUCCEED;
	rspMsg.m_nFileId = req.m_nFileId;
	rspMsg.m_strFileName = req.m_strFileName;
	rspMsg.m_strFriendId = req.m_strFriendId;
	rspMsg.m_strUserId = req.m_strUserId;
	rspMsg.m_strMsgId = req.m_strMsgId;
	pSess->SendMsg(&rspMsg);

}

void CChatServer::HandleFileDownLoadReq(const std::shared_ptr<CServerSess>& pSess, const FileDownLoadReqMsg& req)
{
	if (pSess)
	{
		FileDownLoadRspMsg rspMsg;
		rspMsg.m_strMsgId = req.m_strMsgId;
		rspMsg.m_strUserId = req.m_strUserId;
		rspMsg.m_strFriendId = req.m_strFriendId;
		rspMsg.m_strFileName = req.m_strFileName;
		rspMsg.m_strRelateMsgId = req.m_strRelateMsgId;
		{

			std::string strFileName = m_fileUtil.GetCurDir() + "\\" + req.m_strFriendId + "\\" + req.m_strFileName;
			
			rspMsg.m_strFileHash = m_fileUtil.CalcHash(strFileName);
			if (rspMsg.m_strFileHash.empty())
			{
				rspMsg.m_errCode = ERROR_CODE_TYPE::E_CODE_NO_SUCH_FILE;
				pSess->SendMsg(&rspMsg);
			}
			else
			{
				rspMsg.m_errCode = ERROR_CODE_TYPE::E_CODE_SUCCEED;
				pSess->SendMsg(&rspMsg);

				FileSendDataBeginReq reqMsg;
				reqMsg.m_strUserId = req.m_strUserId;
				reqMsg.m_strFriendId = req.m_strFriendId;
				reqMsg.m_strFileName = req.m_strFileName;
				reqMsg.m_nFileId = rand();
				reqMsg.m_strFileHash = rspMsg.m_strFileHash;
				reqMsg.m_strMsgId = CreateMsgId();
				pSess->SendMsg(&reqMsg);
			}
		}
	}
}
void CChatServer::HandleFileSendDataBeginRsp(const std::shared_ptr<CServerSess>& pSess, const FileSendDataBeginRsp& req)
{
	if (req.m_errCode == ERROR_CODE_TYPE::E_CODE_SUCCEED)
	{
		std::string strFileName = m_fileUtil.GetCurDir() + "\\" + req.m_strFriendId + "\\" + req.m_strFileName;
		int nFileSize = 0;
		m_fileUtil.GetFileSize(nFileSize, strFileName);
		if (m_fileUtil.OpenReadFile(req.m_nFileId, strFileName)) {
			FileDataRecvReqMsg sendReqMsg;
			sendReqMsg.m_strMsgId = std::to_string(m_MsgID_Util.nextId());
			sendReqMsg.m_strFriendId = req.m_strFriendId;
			sendReqMsg.m_strUserId = req.m_strUserId;
			sendReqMsg.m_nFileId = req.m_nFileId;

			sendReqMsg.m_nDataTotalCount = nFileSize / 1024 + (nFileSize % 1024 == 0 ? 0 : 1);
			sendReqMsg.m_nDataIndex = 1;
			sendReqMsg.m_nDataLength = 0;
			m_fileUtil.OnReadData(sendReqMsg.m_nFileId, sendReqMsg.m_szData, sendReqMsg.m_nDataLength, 1024);
			auto item = m_userIdUdpAddrMap.find(sendReqMsg.m_strUserId);
			if (item != m_userIdUdpAddrMap.end())
			{
				m_udpServer->sendMsg(item->second.m_strServerIp, item->second.m_nPort, &sendReqMsg);
			}
			else
			{

			}
		}
	}
}

/**
 * @brief 处理好友发送文本消息
 * 
 * @param pSess 用户连接会话
 * @param reqMsg 发送文本消息请求
 */
void CChatServer::HandleFriendChatSendTxtReq(const std::shared_ptr<CServerSess>& pSess, const FriendChatSendTxtReqMsg& reqMsg)
{
	FriendChatSendTxtRspMsg rspMsg = DoFriendChatSendTxtReq(reqMsg);
	rspMsg.m_strMsgId = reqMsg.m_strMsgId;
	if (pSess)
	{
		pSess->SendMsg(&rspMsg);
	}
	//OnUserReceiveMsg(reqMsg.m_strSenderId);
	//OnUserReceiveMsg(reqMsg.m_strReceiverId);
	OnUserStateCheck(reqMsg.m_strSenderId);
	OnUserStateCheck(reqMsg.m_strReceiverId);
}

/**
 * @brief 处理查找好友请求
 * 
 * @param pSess 用户会话
 * @param reqMsg 查找好友请求
 */
void CChatServer::HandleFindFriendReq(const std::shared_ptr<CServerSess>& pSess, const FindFriendReqMsg& reqMsg)
{
	FindFriendRspMsg rspMsg = DoFindFriendReq(reqMsg);
	rspMsg.m_strMsgId = reqMsg.m_strMsgId;
	if (pSess)
	{
		pSess->SendMsg(&rspMsg);
	}
}


/**
 * @brief 响应更新好友列表回复
 * 
 * @param pSess 用户会话
 * @param reqMsg 更新好友列表回复
 */
void CChatServer::HandleUpdateFriendListRsp(const std::shared_ptr<CServerSess>& pSess, const UpdateFriendListNotifyRspMsg& reqMsg)
{
	OnUserStateCheck(reqMsg.m_strUserId);
}

/**
 * @brief 处理更新群组列表回复
 * 
 * @param pSess 用户会话
 * @param reqMsg 更新群列表回复
 */
void CChatServer::HandleUpdateGroupListRsp(const std::shared_ptr<CServerSess>& pSess, const UpdateGroupListNotifyRspMsg& reqMsg)
{
	OnUserRecvGroupMsg(reqMsg.m_strUserId);
}

/**
 * @brief 处理获取好友列表请求
 * 
 * @param pSess 用户会话
 * @param reqMsg 获取好友列表请求
 */
void CChatServer::HandleGetFriendListReq(const std::shared_ptr<CServerSess>& pSess, const GetFriendListReqMsg& reqMsg) {
	GetFriendListRspMsg rspMsg = DoGetFriendReq(reqMsg);
	rspMsg.m_strMsgId = reqMsg.m_strMsgId;
	if (pSess)
	{
		pSess->SendMsg(&rspMsg);
	}
	//获取好友列表以后，再将未读消息下发，聊天窗口中就不会没有昵称了
	//OnUserStateCheck(reqMsg.m_strUserId);
}


/**
 * @brief 处理创建群组请求
 * 
 * @param pSess 用户会话
 * @param reqMsg 创建群组请求
 */
void CChatServer::HandleCreateGroupReq(const std::shared_ptr<CServerSess>& pSess, const CreateGroupReqMsg& reqMsg)
{
	CreateGroupRspMsg rspMsg = DoCreateGroupReq(reqMsg);
	rspMsg.m_strMsgId = reqMsg.m_strMsgId;
	if (pSess)
	{
		pSess->SendMsg(&rspMsg);
	}
}


/**
 * @brief 处理解散群组请求
 * 
 * @param pSess 用户会话
 * @param reqMsg 解散群组请求
 */
void CChatServer::HandleDestroyGroupReq(const std::shared_ptr<CServerSess>& pSess, const DestroyGroupReqMsg& reqMsg)
{
	DestroyGroupRspMsg rspMsg = DoDestroyGroupReq(reqMsg);
	rspMsg.m_strMsgId = reqMsg.m_strMsgId;
	if (pSess)
	{
		pSess->SendMsg(&rspMsg);
	}
}

/**
 * @brief 处理创建群组请求
 * 
 * @param reqMsg 群组创建请求
 * @return CreateGroupRspMsg 群组创建回复
 */
CreateGroupRspMsg CChatServer::DoCreateGroupReq(const CreateGroupReqMsg& reqMsg)
{
	CreateGroupRspMsg rspMsg;

	if (reqMsg.m_strGroupName.empty())
	{
		rspMsg.m_eErrCode = ERROR_CODE_TYPE::E_CODE_EMPTY_GROUP_NAME;
		return rspMsg;
	}

	if (!m_util.IsUserExist(reqMsg.m_strUserId))
	{
		rspMsg.m_eErrCode = ERROR_CODE_TYPE::E_CODE_NO_SUCH_USER;
		return rspMsg;
	}
	bool bGood = true;
	std::string strGroupId = GenerateUserId();

	if(bGood)
	{
		T_GROUP_BEAN groupBean;
		groupBean.m_strF_GROUP_ID = strGroupId;
		groupBean.m_strF_GROUP_NAME = EncodeUtil::AnsiToUtf8(reqMsg.m_strGroupName);

		if (m_util.InsertGroup(groupBean))
		{

		}
		else
		{
			bGood = false;
		}
	}

	if(bGood)
	{
		T_GROUP_RELATION_BEAN relationBean;
		relationBean.m_eRole = E_GROUP_MEMBER_ROLE::E_ROLE_CREATER;
		relationBean.m_strF_USER_ID = reqMsg.m_strUserId;
		relationBean.m_strF_GROUP_ID = strGroupId;
		if (m_util.InsertGroupRelation(relationBean))
		{

		}
		else
		{
			bGood = false;
		}
	}

	if (bGood)
	{
		rspMsg.m_eErrCode = ERROR_CODE_TYPE::E_CODE_SUCCEED;
		rspMsg.m_strGroupId = strGroupId;
		rspMsg.m_strGroupName = reqMsg.m_strGroupName;
		rspMsg.m_strUserId = reqMsg.m_strUserId;
	}
	else
	{
		rspMsg.m_eErrCode = ERROR_CODE_TYPE::E_CODE_LOGIN_FAILED;
	}

	return rspMsg;
}

/**
 * @brief 处理群组解散请求
 * 
 * @param reqMsg 群组解散请求
 * @return DestroyGroupRspMsg 群组解散回复 
 */
DestroyGroupRspMsg CChatServer::DoDestroyGroupReq(const DestroyGroupReqMsg& reqMsg)
{
	DestroyGroupRspMsg rspMsg;
	T_GROUP_BEAN bean;
	rspMsg.m_eErrorCode = ERROR_CODE_TYPE::E_CODE_SUCCEED;
	m_util.DeleteGroup(reqMsg.m_strGroupId);
	return rspMsg;
}

/**
 * @brief 实际处理查找好友请求
 * 
 * @param reqMsg 查找好友请求
 * @return FindFriendRspMsg 查找好友回复
 */
FindFriendRspMsg  CChatServer::DoFindFriendReq(const FindFriendReqMsg& reqMsg) {
	FindFriendRspMsg rspMsg;
	rspMsg.m_eErrCode = ERROR_CODE_TYPE::E_CODE_SUCCEED;
	T_USER_INFO_BEAN infoBean;
	if (m_util.SelectUserInfoByName(reqMsg.m_strWantedName, infoBean)) {
		UserBaseInfo info;
		{
			info.m_strUserId = infoBean.m_strF_USER_ID;
			info.m_strAddress = infoBean.m_strF_ADDRESS;
			info.m_strBirthDate = infoBean.m_strF_BIRTH_DATE;
			info.m_strEmail = infoBean.m_strF_EMAIL_ADDR;
			info.m_strNickName = infoBean.m_strF_NICK_NAME;
			info.m_strSignature = infoBean.m_strF_SIGNATURE;
			info.m_strFaceId = infoBean.m_strF_FACE_ID;
			info.m_strUserName = infoBean.m_strF_USER_NAME;
		}
		rspMsg.m_friendInfoVec.push_back(info);
	}
	return rspMsg;
}

/**
 * @brief 处理用户收到消息的回复
 * 
 * @param pSess 用户连接会话
 * @param regMsg 收到消息的回复
 */
void CChatServer::HandleFriendChatRecvMsgRsp(const std::shared_ptr<CServerSess>& pSess, const FriendChatRecvTxtRspMsg& regMsg) {
	m_util.UpdateFriendChatMsgState(regMsg.m_strChatMsgId, "READ");
	OnUserReceiveMsg(pSess->UserId());
}

/**
 * @brief 处理好友之间的发送文件请求
 * 
 * @param pSess 用户会话
 * @param reqMsg 发送文件请求消息
 */
void CChatServer::HandleFriendSendFileReq(const std::shared_ptr<CServerSess>& pSess, const FriendSendFileMsgReqMsg& reqMsg)
{
	//For Sender 对于发送者
	{
		auto item = m_UserSessVec.find(reqMsg.m_strToId);
		if (item != m_UserSessVec.end())
		{
			//接收方在线
			{
				FriendSendFileMsgRspMsg rspMsg;

				rspMsg.m_eErrCode = ERROR_CODE_TYPE::E_CODE_SUCCEED;
				rspMsg.m_strMsgId = reqMsg.m_strMsgId;
				rspMsg.m_strFromId = reqMsg.m_strFromId;
				rspMsg.m_strToId = reqMsg.m_strToId;
				pSess->SendMsg(&rspMsg);
			}

			{
				FriendRecvFileMsgReqMsg recvMsg;
				recvMsg.m_strMsgId = reqMsg.m_strMsgId;
				recvMsg.m_strFromId = reqMsg.m_strFromId;
				recvMsg.m_strToId = reqMsg.m_strToId;
				recvMsg.m_strFileName = reqMsg.m_strFileName;
				pSess->SendMsg(&recvMsg);
			}
		}
		else
		{
			FriendSendFileMsgRspMsg rspMsg;

			rspMsg.m_eErrCode = ERROR_CODE_TYPE::E_CODE_USER_OFF_LINE;
			rspMsg.m_strMsgId = reqMsg.m_strMsgId;
			rspMsg.m_strFromId = reqMsg.m_strFromId;
			rspMsg.m_strToId = reqMsg.m_strToId;
			pSess->SendMsg(&rspMsg);
		}
	}
}

/**
 * @brief 处理好友接收文件的回复消息
 * 
 * @TODO: 没有区分离线和在线的情况
 * 
 * @param pSess 用户会话
 * @param reqMsg 接收文件回复消息
 */
void CChatServer::HandleFriendRecvFileRsp(const std::shared_ptr<CServerSess>& pSess, const FriendRecvFileMsgRspMsg& reqMsg)
{
	//
	{
		auto item = m_UserSessVec.find(reqMsg.m_strToId);
		if (item != m_UserSessVec.end())
		{
			FriendNotifyFileMsgReqMsg sendMsg;
			sendMsg.m_strMsgId = reqMsg.m_strMsgId;
			sendMsg.m_eOnlineType = reqMsg.m_eOnlineType;
			sendMsg.m_eOption = reqMsg.m_eOption;
			sendMsg.m_strFromId = reqMsg.m_strFromId;
			sendMsg.m_strToId = reqMsg.m_strToId;
			sendMsg.m_strFileName = reqMsg.m_strFileName;
			sendMsg.m_nFileId = reqMsg.m_nFileId;
			item->second->SendMsg(&sendMsg);
		}
	}
	
}

/**
 * @brief 处理好友接收文件的通知回复消息
 * 
 * @param pSess 
 * @param reqMsg 接收文件的通知回复消息
 */
void CChatServer::HandleFriendNotifyFileRsp(const std::shared_ptr<CServerSess>& pSess, const FriendNotifyFileMsgRspMsg& reqMsg)
{

}

/**
 * @brief 通知用户更新好友列表
 * TODO: 函数需要重命名
 * @param strUserId 用户ID
 */
void CChatServer::NotifyUserFriends(const std::string strUserId)
{
	std::vector<std::string> strFriendIdList;
	if (m_util.GetUserFriendList(strUserId, strFriendIdList))
	{
		for (auto item : strFriendIdList)
		{
			auto findItem = m_UserSessVec.find(item);
			if (findItem != m_UserSessVec.end() && findItem->second->IsConnected())
			{
				UpdateFriendListNotifyReqMsg reqMsg;
				reqMsg.m_strUserId = item;
				reqMsg.m_strMsgId = std::to_string(m_MsgID_Util.nextId());
				findItem->second->SendMsg(&reqMsg);
			}
		}
	}
	
}
/**
 * @brief 处理获取群组请求
 * 
 * @param pSess 用户会话
 * @param reqMsg 获取用户群组请求
 */
void CChatServer::HandleGetGroupListReq(const std::shared_ptr<CServerSess>& pSess, const GetGroupListReqMsg& reqMsg)
{
	GetGroupListRspMsg rspMsg = DoGetGroupListReq(reqMsg);
	rspMsg.m_strMsgId = reqMsg.m_strMsgId;
	if (pSess)
	{
		pSess->SendMsg(&rspMsg);
	}
}

/**
 * @brief 处理用户退出群聊请求
 * 
 * @param pSess 
 * @param reqMsg 
 */
void CChatServer::HandleQuitGroupReqMsg(const std::shared_ptr<CServerSess>& pSess, const QuitFromGroupReqMsg& reqMsg)
{
	QuitFromGroupRspMsg rspMsg = DoQuitFromGroup(reqMsg);
	rspMsg.m_strMsgId = reqMsg.m_strMsgId;
	if (pSess)
	{
		pSess->SendMsg(&rspMsg);
	}
}

/**
 * @brief 执行用户退出群聊请求
 * 
 * @param reqMsg 用户退出群聊请求 
 * @return QuitFromGroupRspMsg 用户退出群聊回复
 */
QuitFromGroupRspMsg CChatServer::DoQuitFromGroup(const QuitFromGroupReqMsg& reqMsg)
{
	QuitFromGroupRspMsg rspMsg;
	rspMsg.m_strGroupId = reqMsg.m_strGroupId;
	rspMsg.m_strUserId = reqMsg.m_strUserId;
	rspMsg.m_strMsgId = reqMsg.m_strMsgId;
	T_GROUP_RELATION_BEAN bean;
	bean.m_strF_USER_ID = reqMsg.m_strUserId;
	bean.m_strF_GROUP_ID = reqMsg.m_strGroupId;
	if (m_util.DeleteGroupRelation(bean))
	{

	}

	return rspMsg;
}
/**
 * @brief 处理获取群组列表请求
 * 
 * @param reqMsg 获取群组列表请求
 * @return GetGroupListRspMsg 获取群组列表回复
 */
GetGroupListRspMsg CChatServer::DoGetGroupListReq(const GetGroupListReqMsg& reqMsg)
{
	GetGroupListRspMsg rspMsg;
	//查找该用户所在的所有群组
	std::vector<T_GROUP_RELATION_BEAN> userGroups;
	if (m_util.SelectUserGroupRelation(reqMsg.m_strUserId,userGroups)) {
		//
		GroupInfo groupInfo;
		for (const auto groupIdItem : userGroups)
		{
			T_GROUP_BEAN groupBean;

			if(m_util.SelectGroupById(groupIdItem.m_strF_GROUP_ID,groupBean))
			{
				groupInfo.m_strGroupName = groupBean.m_strF_GROUP_NAME;
				groupInfo.m_strGroupId = groupIdItem.m_strF_GROUP_ID;
				std::vector<T_GROUP_RELATION_BEAN> groupUsers;
				if (m_util.SelectGroupRelation(groupIdItem.m_strF_GROUP_ID, groupUsers))
				{
					for (const auto userRelationItem : groupUsers) {
						T_USER_INFO_BEAN userBean;
						UserBaseInfo baseInfo;
						if (m_util.SelectUserInfoByName(userRelationItem.m_strF_USER_ID, userBean)) {
							{
								baseInfo.m_strUserName = userBean.m_strF_USER_NAME;
								baseInfo.m_strAddress = userBean.m_strF_ADDRESS;
								baseInfo.m_strBirthDate = userBean.m_strF_BIRTH_DATE;
								baseInfo.m_strEmail = userBean.m_strF_EMAIL_ADDR;
								baseInfo.m_strFaceId = userBean.m_strF_FACE_ID;
								baseInfo.m_strNickName = userBean.m_strF_NICK_NAME;
								baseInfo.m_strSignature = userBean.m_strF_SIGNATURE;
								baseInfo.m_strUserId = userBean.m_strF_USER_ID;
							}
							groupInfo.m_GroupUsers.push_back(baseInfo);
						}
					}
				}
			}
			rspMsg.m_GroupList.push_back(groupInfo);
			groupInfo.m_GroupUsers.clear();
		}
	}
	rspMsg.m_strUserId = reqMsg.m_strUserId;
	return rspMsg;
}


/**
 * @brief 处理用户添加好友的请求
 * 
 * @param pSess 用户连接会话
 * @param reqMsg 添加好友请求
 */
void CChatServer::HandleAddFriendReq(const std::shared_ptr<CServerSess>& pSess, const AddFriendSendReqMsg& reqMsg) {
	AddFriendSendRspMsg rspMsg = DoAddFriendReq(reqMsg);
	rspMsg.m_strMsgId = reqMsg.m_strMsgId;
	if (pSess)
	{
		pSess->SendMsg(&rspMsg);
	}

	OnAddFriendRecvReqMsg(reqMsg.m_strFriendId);
}

/**
 * @brief 处理添加好友的Recv回复消息
 * 
 * @param pSess 用户会话
 * @param rspMsg 添加好友的Recv消息[好友接收方---->服务器]
 */
void CChatServer::HandleAddFriendRecvRsp(const std::shared_ptr<CServerSess>& pSess, const AddFriendRecvRspMsg& rspMsg)
{
	T_ADD_FRIEND_MSG_BEAN bean;
	m_util.UpdateToReadUnNotifyAddFriendMsg(rspMsg.m_strMsgId, rspMsg.m_option);
	OnAddFriendNotifyReqMsg(rspMsg.m_strFriendId);
}

/**
 * @brief 处理添加好友通知回复消息
 * 
 * @param pSess 用户会话
 * @param rspMsg 添加好友通知回复消息[发起方--->服务器]
 */
void CChatServer::HandleAddFriendNotifyRsp(const std::shared_ptr<CServerSess>& pSess, const AddFriendNotifyRspMsg& rspMsg)
{
	m_util.UpdateToNotifyAddFriendMsg(rspMsg.m_strMsgId);
}

/**
 * @brief 实际处理删除好友请求
 * 
 * @param reqMsg 删除好友请求
 * @return RemoveFriendRspMsg 删除好友回复
 */
RemoveFriendRspMsg CChatServer::DoRemoveFriendReq(const RemoveFriendReqMsg& reqMsg)
{
	RemoveFriendRspMsg rspMsg;
	rspMsg.m_strUserId = reqMsg.m_strUserId;
	rspMsg.m_strFriendId = reqMsg.m_strFriendId;
	rspMsg.m_eErrCode = ERROR_CODE_TYPE::E_CODE_NO_SUCH_USER;
	//正
	if (m_util.IsFriend(reqMsg.m_strUserId, reqMsg.m_strFriendId))
	{
		m_util.DeleteFriendRelation(reqMsg.m_strUserId, reqMsg.m_strFriendId);
		rspMsg.m_eErrCode = ERROR_CODE_TYPE::E_CODE_SUCCEED;
	}
	//反
	if (m_util.IsFriend(reqMsg.m_strFriendId, reqMsg.m_strUserId))
	{
		m_util.DeleteFriendRelation(reqMsg.m_strFriendId, reqMsg.m_strUserId);
		rspMsg.m_eErrCode = ERROR_CODE_TYPE::E_CODE_SUCCEED;
	}

	rspMsg.m_strErrMsg = "Succeed";
	return rspMsg;
}

/**
 * @brief 处理删除好友的消息
 * 
 * @param pSess 用户连接会话
 * @param reqMsg 删除好友的消息
 */
void CChatServer::HandleRemoveFriendReq(const std::shared_ptr<CServerSess>& pSess, const RemoveFriendReqMsg& reqMsg) {
	RemoveFriendRspMsg rspMsg = DoRemoveFriendReq(reqMsg);
	rspMsg.m_strMsgId = reqMsg.m_strMsgId;
	rspMsg.m_eErrCode = ERROR_CODE_TYPE::E_CODE_SUCCEED;
	rspMsg.m_strUserId = reqMsg.m_strUserId;
	rspMsg.m_strFriendId = reqMsg.m_strFriendId;
	if (pSess)
	{
		pSess->SendMsg(&rspMsg);
	}
}


/**
 * @brief 处理增加分组请求
 * 
 * @param pSess 用户会话
 * @param reqMsg 增加分组请求
 */
void CChatServer::HandleAddTeamReq(const std::shared_ptr<CServerSess>& pSess, const AddTeamReqMsg& reqMsg) {
	AddTeamRspMsg rspMsg = DoAddTeamReq(reqMsg);
	rspMsg.m_strMsgId = reqMsg.m_strMsgId;
	if (pSess)
	{
		pSess->SendMsg(&rspMsg);
	}
}


/**
 * @brief 处理删除分组请求
 * 
 * @param pSess 用户会话
 * @param reqMsg 移动分组请求
 */
void CChatServer::HandleRemoveTeamReq(const std::shared_ptr<CServerSess>& pSess, const RemoveTeamReqMsg& reqMsg) {
	RemoveTeamRspMsg rspMsg = DoRemoveTeamReq(reqMsg);
	rspMsg.m_strMsgId = reqMsg.m_strMsgId;
	if (pSess)
	{
		pSess->SendMsg(&rspMsg);
	}
}

/**
 * @brief 处理好友移动分组请求
 * 
 * @param pSess 用户连接会话
 * @param reqMsg 好友移动分组请求
 */
void CChatServer::HandleMoveFriendToTeamReq(const std::shared_ptr<CServerSess>& pSess, const MoveFriendToTeamReqMsg& reqMsg) {
	MoveFriendToTeamRspMsg rspMsg = DoMoveFriendToTeamReq(reqMsg);
	rspMsg.m_strMsgId = reqMsg.m_strMsgId;
	if (pSess)
	{
		pSess->SendMsg(&rspMsg);
	}
}



/**
 * @brief 发送用户接收到的消息
 * 
 * @param pSess 用户连接会话
 * @param reqMsg 用户文本消息
 */
void CChatServer::HandleFriendChatRecvMsgReq(const std::shared_ptr<CServerSess>& pSess, const FriendChatRecvTxtReqMsg& reqMsg)
{
	if (pSess)
	{
		pSess->SendMsg(&reqMsg);
	}
}


/**
 * @brief 处理查找群组请求
 * 
 * @param pSess 用户连接会话
 * @param reqMsg 查找群组请求
 */
void CChatServer::HandleFindGroupReq(const std::shared_ptr<CServerSess>& pSess, const FindGroupReqMsg& reqMsg)
{
	FindGroupRspMsg rspMsg = DoFindGroupReq(reqMsg);
	rspMsg.m_strMsgId = reqMsg.m_strMsgId;
	if (pSess)
	{
		pSess->SendMsg(&rspMsg);
	}
}

/**
 * @brief 实际处理用户发送文本消息的请求
 * 
 * @param reqMsg 用户发送文本消息
 * @return FriendChatSendTxtRspMsg 用户发送文本消息的回复
 */
FriendChatSendTxtRspMsg CChatServer::DoFriendChatSendTxtReq(const FriendChatSendTxtReqMsg& reqMsg)
{
	FriendChatSendTxtRspMsg rspMsg; 
	if (m_util.IsUserExist(reqMsg.m_strSenderId) && m_util.IsUserExist(reqMsg.m_strSenderId))
	{
		T_USER_CHAT_MSG  msg;
		msg.m_strF_MSG_ID = std::to_string(m_MsgID_Util.nextId());
		msg.m_strF_FROM_ID = reqMsg.m_strSenderId;
		msg.m_strF_TO_ID = reqMsg.m_strReceiverId;
		msg.m_eChatMsgType = CHAT_MSG_TYPE::E_CHAT_TEXT_TYPE;
		msg.m_strF_MSG_CONTEXT = EncodeUtil::AnsiToUtf8(reqMsg.m_strContext);
		msg.m_strF_OTHER_INFO = reqMsg.m_fontInfo.ToString();
		msg.m_strF_CREATE_TIME = CTimeUtil::GetYMD_HMS_Time();
		m_util.InsertFriendChatMsg(msg);
		
		rspMsg.m_eErrCode = ERROR_CODE_TYPE::E_CODE_SUCCEED;
		rspMsg.m_strMsgId = reqMsg.m_strMsgId;
		rspMsg.m_chatMsg = DbBeanToMsgBean(msg);
		rspMsg.m_strErrMsg = "Succeed";
	}
	else
	{
		rspMsg.m_eErrCode = ERROR_CODE_TYPE::E_CODE_NO_SUCH_USER;
	}
	return rspMsg;
}


/**
 * @brief 处理用户登陆请求
 * 
 * @param reqMsg 用户登陆请求
 * @return UserLoginRspMsg 用户登陆回复
 */
UserLoginRspMsg CChatServer::DoUserLoginReq(const UserLoginReqMsg& reqMsg) {
	UserLoginRspMsg rsp;
	T_USER_BEAN userBean;
	m_util.SelectUserByName(reqMsg.m_strUserName, userBean);
	if (VerifyPassword(reqMsg.m_strPassword,userBean.m_strF_PASS_WORD))
	{
		rsp.m_eErrCode = ERROR_CODE_TYPE::E_CODE_SUCCEED;
		rsp.m_strErrMsg = "Succeed";
		rsp.m_strUserName = reqMsg.m_strUserName;

		T_USER_INFO_BEAN infoBean;
		m_util.SelectUserInfoByName(reqMsg.m_strUserName, infoBean);
		{
			rsp.m_userInfo.m_strUserId = infoBean.m_strF_USER_ID;
			rsp.m_userInfo.m_strUserName = infoBean.m_strF_USER_NAME;
			rsp.m_userInfo.m_strAddress = infoBean.m_strF_ADDRESS;
			rsp.m_userInfo.m_strBirthDate = infoBean.m_strF_BIRTH_DATE;
			rsp.m_userInfo.m_strEmail = infoBean.m_strF_EMAIL_ADDR;
			rsp.m_userInfo.m_strFaceId = infoBean.m_strF_FACE_ID;
			rsp.m_userInfo.m_strNickName = infoBean.m_strF_NICK_NAME;
			rsp.m_userInfo.m_strSignature = infoBean.m_strF_SIGNATURE;
		}
		rsp.m_strUserId = infoBean.m_strF_USER_ID;
		m_util.UpdateUserOnlineState(userBean.m_strF_USER_ID, CLIENT_ONLINE_TYPE::C_ONLINE_TYPE_ONLINE);
		return rsp;
	}
	else
	{
		rsp.m_eErrCode = ERROR_CODE_TYPE::E_CODE_LOGIN_FAILED;
		rsp.m_strErrMsg = "Failed";
	}
	return rsp;
}


/**
 * @brief 生成用户ID
 * 
 * @return std::string 用户ID，最长8个字符
 */
std::string CChatServer::GenerateUserId() {
	return std::to_string(m_MsgID_Util.nextId() % 100000000);
}
/**
 * @brief 实际处理用户的注册请求
 * 
 * @param reqMsg 用户的注册请求
 * @return UserRegisterRspMsg 用户注册的回复
 */
UserRegisterRspMsg CChatServer::DoUserRegisterReq(const UserRegisterReqMsg& reqMsg) {
	
	UserRegisterRspMsg rsp;

	T_USER_BEAN userBean;
	if (m_util.SelectUserByName(reqMsg.m_strUserName, userBean))
	{
		rsp.m_eErrCode = ERROR_CODE_TYPE::E_CODE_USER_HAS_EXIST;
		rsp.m_strErrMsg = "Failed";
		rsp.m_strUserName = reqMsg.m_strUserName;
		return rsp;
	}
	else
	{
		T_USER_BEAN newUser;
		newUser.m_strF_USER_ID = GenerateUserId();
		newUser.m_strF_USER_NAME = reqMsg.m_strUserName;
		newUser.m_strF_PASS_WORD = GeneratePassword(reqMsg.m_strPassword);
		newUser.m_strF_NICK_NAME = reqMsg.m_strNickName;
		if (m_util.InsertUser(newUser))
		{
			rsp.m_eErrCode = ERROR_CODE_TYPE::E_CODE_SUCCEED;
			rsp.m_strErrMsg = "Succeed";
			rsp.m_strUserName = newUser.m_strF_USER_NAME;

			T_USER_TEAM_BEAN teamBean;
			teamBean.m_strF_TEAM_ID = DEFAULT_TEAM_ID;
			teamBean.m_strF_TEAM_NAME = DEFAULT_TEAM_NAME;
			teamBean.m_strF_USER_ID = newUser.m_strF_USER_ID;
			m_util.InsertUserTeam(teamBean);
		}
		else
		{
			rsp.m_eErrCode = ERROR_CODE_TYPE::E_CODE_NO_SUCH_USER;
			rsp.m_strErrMsg = "Failed";
			rsp.m_strUserName = newUser.m_strF_USER_NAME;
		}
		return rsp;
	}
}


/**
 * @brief 处理用户注销请求
 * 
 * @param reqMsg 用户注销请求
 * @return UserUnRegisterRspMsg 用户注销回复 
 */
UserUnRegisterRspMsg CChatServer::DoUserUnRegisterReq(const UserUnRegisterReqMsg& reqMsg) {
	UserUnRegisterRspMsg rspMsg;
	T_USER_BEAN userBean;
	m_util.SelectUserByName(reqMsg.m_strUserName, userBean);
	if (userBean.m_strF_PASS_WORD.empty())
	{
		rspMsg.m_eErrCode = ERROR_CODE_TYPE::E_CODE_NO_SUCH_USER;
		return rspMsg;
	}
	else
	{
		if (userBean.m_strF_USER_NAME == reqMsg.m_strUserName && userBean.m_strF_PASS_WORD == reqMsg.m_strPassword)
		{
			T_USER_BEAN newUser;
			m_util.DeleteUser(userBean.m_strF_USER_ID);
		}

		rspMsg.m_eErrCode = ERROR_CODE_TYPE::E_CODE_SUCCEED;
		rspMsg.m_strErrMsg = "Succeed";
		rspMsg.m_strUserName = reqMsg.m_strUserName;
		return rspMsg;
	}
}


/**
 * @brief 处理用户获取好友列表请求
 * 
 * @param req 获取好友列表请求
 * @return GetFriendListRspMsg 获取好友列表回复
 */
GetFriendListRspMsg CChatServer::DoGetFriendReq(const GetFriendListReqMsg& req) {
	std::vector<std::string> strFriendIdList;
	std::vector<T_FRIEND_RELATION_BEAN> friendBeanList;
	std::vector<T_USER_TEAM_BEAN> teamBeanList;
	GetFriendListRspMsg rspMsg;
	rspMsg.m_strUserId = req.m_strUserId;
	if (m_util.SelectUserTeams(req.m_strUserId, teamBeanList))
	{
		if (m_util.GetUserFriendList(req.m_strUserId, friendBeanList))
		{
			
		}
			std::map<std::string,TeamBaseInfo> teamIdTeamMap;
			for (const auto teamItem : teamBeanList)
			{
				TeamBaseInfo teamInfo;
				teamInfo.m_strTeamId = teamItem.m_strF_TEAM_ID;
				teamInfo.m_strTeamName = teamItem.m_strF_TEAM_NAME;
				teamIdTeamMap.insert({ teamItem.m_strF_TEAM_ID,teamInfo });
			}
			{	
				UserBaseInfo info;
				for (const auto& friendItem : friendBeanList) {
					T_USER_INFO_BEAN bean;
					if (m_util.SelectUserInfoByName(friendItem.m_strF_FRIEND_ID, bean)) {
						{
							info.m_strUserId = bean.m_strF_USER_ID;
							info.m_strAddress = bean.m_strF_ADDRESS;
							info.m_strBirthDate = bean.m_strF_BIRTH_DATE;
							info.m_strEmail = bean.m_strF_EMAIL_ADDR;
							info.m_strNickName = bean.m_strF_NICK_NAME;
							info.m_strSignature = bean.m_strF_SIGNATURE;
							info.m_strFaceId = bean.m_strF_FACE_ID;
							info.m_strUserName = bean.m_strF_USER_NAME;
							info.m_eOnlineState = bean.m_eOnlineState;
						}
						{
							auto item = teamIdTeamMap.find(friendItem.m_strF_TEAM_ID);
							if (item != teamIdTeamMap.end())
							{
								item->second.m_teamUsers.push_back(info);
							}
							else
							{
								TeamBaseInfo teamInfo;
								teamInfo.m_strTeamId = DEFAULT_TEAM_ID;
								teamInfo.m_strTeamName = DEFAULT_TEAM_NAME;
								teamInfo.m_teamUsers.push_back(info);
								teamIdTeamMap.insert({ friendItem.m_strF_TEAM_ID,teamInfo });
							}
						}
					}
					//rspMsg.m_friendInfoVec.push_back(info);
				}
			}

			for (const auto item : teamIdTeamMap)
			{
				rspMsg.m_teamVec.push_back(item.second);
			}
	}

	return rspMsg;
}

/**
 * @brief 处理用户退出登陆请求
 * 
 * @param reqMsg 退出登陆请求
 * @return UserLogoutRspMsg 退出登陆回复
 */
UserLogoutRspMsg CChatServer::DoUserLogoutReq(const UserLogoutReqMsg& reqMsg) {
	T_USER_BEAN bean;
	UserLogoutRspMsg rspMsg;
	if (m_util.SelectUserByName(reqMsg.m_strUserName, bean)) {
		if (bean.m_strF_PASS_WORD == reqMsg.m_strPassword) {
			rspMsg.m_eErrCode = ERROR_CODE_TYPE::E_CODE_SUCCEED;
			rspMsg.m_strUserName = reqMsg.m_strUserName;
		}
	}

	return rspMsg;
}


/**
 * @brief 处理添加好友请求
 * 
 * @param reqMsg 添加好友请求
 * @return AddFriendSendRspMsg 添加好友回复
 */
AddFriendSendRspMsg CChatServer::DoAddFriendReq(const AddFriendSendReqMsg& reqMsg) {
	AddFriendSendRspMsg rspMsg;
	
	if (reqMsg.m_strUserId == reqMsg.m_strFriendId)
	{
		rspMsg.m_eErrCode = ERROR_CODE_TYPE::E_CODE_ADD_SELF_AS_FRIEND;
	}
	else if (m_util.IsFriend(reqMsg.m_strUserId,reqMsg.m_strFriendId))
	{
		rspMsg.m_strUserId = reqMsg.m_strUserId;
		rspMsg.m_strFriendId = reqMsg.m_strFriendId;
		rspMsg.m_eErrCode = ERROR_CODE_TYPE::E_CODE_SUCCEED;
		rspMsg.m_strErrMsg = "Succeed";
	}
	else
	{
		T_ADD_FRIEND_MSG_BEAN msgBean;
		msgBean.m_strF_MSG_ID = std::to_string(m_MsgID_Util.nextId());
		msgBean.m_strF_USER_ID = reqMsg.m_strUserId;
		msgBean.m_strF_FRIEND_ID = reqMsg.m_strFriendId;
		msgBean.m_eF_FRIEND_OPTION = E_FRIEND_OPTION::E_UN_KNOWN;
		msgBean.m_eF_ADD_FRIEND_STATUS = E_ADD_FRIEND_STATUS::E_UN_READ;
		m_util.InsertAddFriendMsg(msgBean);

		rspMsg.m_eErrCode = ERROR_CODE_TYPE::E_CODE_SUCCEED;
		rspMsg.m_strErrMsg = "Succeed";
	}
	
	return rspMsg;
}

/**
 * @brief 响应好友添加的结果通知消息，
 * 
 * @param strUser 添加好友的发起方用户名
 * @return true 成功
 * @return false 失败
 */
bool CChatServer::OnAddFriendNotifyReqMsg(const std::string strUser)
{
	T_ADD_FRIEND_MSG_BEAN msgBean;
	if (m_util.SelectUnNotifyAddFriendMsg(strUser, msgBean))
	{
		{
			AddFriendNotifyReqMsg reqMsg;

			reqMsg.m_strMsgId = msgBean.m_strF_MSG_ID;
			reqMsg.m_strUserId = strUser;
			reqMsg.m_strFriendId = msgBean.m_strF_FRIEND_ID;

			reqMsg.m_option = msgBean.m_eF_FRIEND_OPTION;
			if ((!m_util.IsFriend(reqMsg.m_strUserId, reqMsg.m_strFriendId))||
				(!m_util.IsFriend(reqMsg.m_strFriendId, reqMsg.m_strUserId) ))
			{
				{
					auto item = m_UserSessVec.find(strUser);
					if (item != m_UserSessVec.end() && item->second->IsConnected())
					{
						item->second->SendMsg(&reqMsg);
					}
				}
				if (E_FRIEND_OPTION::E_AGREE_ADD == reqMsg.m_option)
				{
					T_FRIEND_RELATION_BEAN bean;
					bean.m_eF_STATUS = E_FRIEND_RELATION::E_FRIEND_TYPE;
					bean.m_strF_TEAM_ID = DEFAULT_TEAM_ID;
					
					bean.m_strF_USER_ID = reqMsg.m_strUserId;
					bean.m_strF_FRIEND_ID = reqMsg.m_strFriendId;
					m_util.InsertFriendRelation(bean);
					
					bean.m_strF_FRIEND_ID = reqMsg.m_strUserId;
					bean.m_strF_USER_ID = reqMsg.m_strFriendId;
					m_util.InsertFriendRelation(bean);
				}
				else
				{
					LOG_INFO(ms_loger, "{}  {} Not Friend [ {}  {} ]", reqMsg.m_strUserId, reqMsg.m_strFriendId, __FILENAME__, __LINE__);
				}
			}
		}
		
		return true;
	}
	else {
		return false;
	}
}

/**
 * @brief 处理增加好友分组请求
 * 
 * @param reqMsg　 增加好友分组请求
 * @return AddTeamRspMsg 增加好友分组回复
 */
AddTeamRspMsg CChatServer::DoAddTeamReq(const AddTeamReqMsg& reqMsg) {
	AddTeamRspMsg rspMsg;

	T_USER_TEAM_BEAN teamBean;
	teamBean.m_strF_TEAM_ID = std::to_string(m_MsgID_Util.nextId()%10000000);
	teamBean.m_strF_USER_ID = reqMsg.m_strUserId;
	teamBean.m_strF_TEAM_NAME = reqMsg.m_strTeamName;
	
	if (m_util.InsertUserTeam(teamBean)) {
		rspMsg.m_eErrCode = ERROR_CODE_TYPE::E_CODE_SUCCEED;
		rspMsg.m_strErrMsg = "Succeed";
		rspMsg.m_strTeamId = teamBean.m_strF_TEAM_ID;
		rspMsg.m_strUserId = teamBean.m_strF_USER_ID;
		rspMsg.m_strTeamName = teamBean.m_strF_TEAM_NAME;
	}
	else {
		rspMsg.m_eErrCode = ERROR_CODE_TYPE::E_CODE_NO_SUCH_USER;
		rspMsg.m_strErrMsg = "Failed";
	}
	
	return rspMsg;
}

/**
 * @brief 处理删除分组请求
 * 
 * @param reqMsg 删除分组请求
 * @return RemoveTeamRspMsg 删除分组回复
 */
RemoveTeamRspMsg CChatServer::DoRemoveTeamReq(const RemoveTeamReqMsg& reqMsg) {
	RemoveTeamRspMsg rspMsg;
	T_USER_TEAM_BEAN teamBean;
	teamBean.m_strF_TEAM_ID = reqMsg.m_strTeamId;
	teamBean.m_strF_USER_ID = reqMsg.m_strUserId;

	if (m_util.DeleteUserTeam(teamBean)) {
		rspMsg.m_eErrorCode = ERROR_CODE_TYPE::E_CODE_SUCCEED;
		rspMsg.m_strErrMsg = "Succeed";
		rspMsg.m_strTeamId = reqMsg.m_strTeamId;
		rspMsg.m_strUserId = reqMsg.m_strUserId;
	}
	else {
		rspMsg.m_eErrorCode = ERROR_CODE_TYPE::E_CODE_LOGIN_FAILED;
		rspMsg.m_strErrMsg = "Failed";
	}

	{
		if (m_util.UpdateFriendTeamId(reqMsg.m_strUserId, reqMsg.m_strTeamId, DEFAULT_TEAM_ID))
		{

		}
		else
		{
			LOG_ERR(ms_loger, "Update Friend Team Id Failed User:{} OldTeam:{} NewTeam:{} [{} {}]", reqMsg.m_strUserId, reqMsg.m_strTeamId, DEFAULT_TEAM_ID, __FILENAME__, __LINE__);
		}
	}
	return rspMsg;
}

/**
 * @brief 处理好友移动分组请求
 * 
 * @param reqMsg 好友移动分组请求
 * @return MoveFriendToTeamRspMsg 好友移动分组回复
 */
MoveFriendToTeamRspMsg CChatServer::DoMoveFriendToTeamReq(const MoveFriendToTeamReqMsg& reqMsg) {
	MoveFriendToTeamRspMsg rspMsg;
	T_FRIEND_RELATION_BEAN bean;
	bean.m_strF_USER_ID = reqMsg.m_strUserId;
	bean.m_strF_FRIEND_ID = reqMsg.m_strFriendId;
	bean.m_strF_TEAM_ID = reqMsg.m_strDstTeamId;
	if (m_util.UpdateFriendRelation(bean))
	{
		rspMsg.m_eErrCode = ERROR_CODE_TYPE::E_CODE_SUCCEED;
	}
	else
	{
		rspMsg.m_eErrCode = ERROR_CODE_TYPE::E_CODE_NO_SUCH_USER;
	}
	
	return rspMsg;
}

/**
 * @brief 处理查找群组请求
 * 
 * @param reqMsg 查找群组请求
 * @return FindGroupRspMsg 查找群组回复
 */
FindGroupRspMsg CChatServer::DoFindGroupReq(const FindGroupReqMsg& reqMsg) {
	FindGroupRspMsg rspMsg;
	std::vector<T_GROUP_BEAN> groupBeans;
	if (m_util.SelectGroups(reqMsg.m_strGroupName, groupBeans)) {
		rspMsg.m_eErrCode = ERROR_CODE_TYPE::E_CODE_SUCCEED;
		rspMsg.m_strGroupId = groupBeans[0].m_strF_GROUP_ID;
		rspMsg.m_strGroupName = groupBeans[0].m_strF_GROUP_NAME;
		rspMsg.m_strUserId = reqMsg.m_strUserId;
	}
	else
	{
		rspMsg.m_eErrCode = ERROR_CODE_TYPE::E_CODE_LOGIN_FAILED;
	}
	return rspMsg;
}

/**
 * @brief 处理用户登录以后，下发没有收到的群消息
 * 
 * @param strUser 用户名
 * @return true 成功
 * @return false 失败
 */
bool CChatServer::OnUserRecvGroupMsg(const std::string strUser)
{
	std::vector<T_GROUP_RELATION_BEAN> userGroups;
	if (m_util.SelectUserGroupRelation(strUser, userGroups)) {
		for (auto item : userGroups) {
			T_GROUP_CHAT_MSG msgBean;
			msgBean.m_strF_GROUP_ID = item.m_strF_GROUP_ID;
			msgBean.m_strF_MSG_ID = item.m_strF_LAST_READ_MSG_ID;
			if (m_util.SelectGroupChatText(msgBean))
			{
				if (CHAT_MSG_TYPE::E_CHAT_TEXT_TYPE == msgBean.m_eChatMsgType)
				{
					OnUserRecvGroupMsg(item.m_strF_USER_ID, msgBean);
				}
			}
		}
	}
	return false;
}

/**
 * @brief 处理群聊文本消息
 * 
 * @param pSess 用户会话
 * @param reqMsg 群聊文本消息
 */
void CChatServer::HandleSendGroupTextReq(const std::shared_ptr<CServerSess>& pSess, const SendGroupTextMsgReqMsg& reqMsg)
{
	SendGroupTextMsgRspMsg rspMsg = DoSendGroupTextMsgReqMsg(reqMsg);
	rspMsg.m_strMsgId = reqMsg.m_strMsgId;
	if (pSess)
	{
		pSess->SendMsg(&rspMsg);
	}

	OnDispatchGroupMsg(reqMsg.m_strGroupId);
}

/**
 * @brief 处理用户加入群组请求
 * 
 * @param pSess 用户会话
 * @param reqMsg 加入群组请求消息
 */
void CChatServer::HandleAddToGroupReq(const std::shared_ptr<CServerSess>& pSess, const AddToGroupReqMsg& reqMsg)
{
	AddToGroupRspMsg rspMsg = DoAddToGroupReqMsg(reqMsg);
	rspMsg.m_strMsgId = reqMsg.m_strMsgId;
	if (pSess)
	{
		pSess->SendMsg(&rspMsg);
	}
}

/**
 * @brief 处理查询用户的UDP地址的请求
 * 
 * @param pSess 会话连接
 * @param reqMsg 查询用户的UDP地址的请求消息
 */
void CChatServer::HandleQueryUserUdpAddr(const std::shared_ptr<CServerSess>& pSess, const QueryUserUdpAddrReqMsg& reqMsg)
{
	QueryUserUdpAddrRspMsg rspMsg;
	rspMsg.m_strUdpUserId = reqMsg.m_strUdpUserId;
	rspMsg.m_strUserId = reqMsg.m_strUserId;
	auto item = m_userIdUdpAddrMap.find(reqMsg.m_strUdpUserId);
	if (item != m_userIdUdpAddrMap.end()) {
		rspMsg.m_errCode = ERROR_CODE_TYPE::E_CODE_SUCCEED;
		rspMsg.m_udpEndPt = item->second;
	}
	else
	{
		rspMsg.m_errCode = ERROR_CODE_TYPE::E_CODE_NO_SUCH_USER;
	}
	pSess->SendMsg(&rspMsg);
}


/**
 * @brief 处理UDP端口发送的文件数据发送请求
 * 
 * TODO: 此处需要修改,以方便用于接收群组的文件消息
 * @param reqMsg 文件数据发送请求消息
 */
void CChatServer::Handle_UdpFileDataSendReqMsg(const asio::ip::udp::endpoint sendPt,const FileDataSendReqMsg& reqMsg)
{
	m_fileUtil.OnWriteData(reqMsg.m_nFileId, reqMsg.m_szData, reqMsg.m_nDataLength);
	if (reqMsg.m_nDataIndex == reqMsg.m_nDataTotalCount)
	{
		m_fileUtil.OnCloseFile(reqMsg.m_nFileId);
	}
	//auto item = m_userIdUdpAddrMap.find(reqMsg.m_strToId);
	//if (item != m_userIdUdpAddrMap.end())
	//{
	//	FileDataRecvReqMsg sendReqMsg;
	//	sendReqMsg.m_strMsgId = reqMsg.m_strMsgId;
	//	sendReqMsg.m_strFromId = reqMsg.m_strFromId;
	//	sendReqMsg.m_strToId = reqMsg.m_strToId;
	//	sendReqMsg.m_nFileId = reqMsg.m_nFileId;
	//	sendReqMsg.m_nDataTotalCount = reqMsg.m_nDataTotalCount;
	//	sendReqMsg.m_nDataIndex = reqMsg.m_nDataIndex;
	//	sendReqMsg.m_nDataLength = reqMsg.m_nDataLength;
	//	memcpy(sendReqMsg.m_szData, reqMsg.m_szData, reqMsg.m_nDataLength);
	//	m_udpServer->sendMsg(item->second.m_strServerIp, item->second.m_nPort, &sendReqMsg);
	//}
	//else
	{
		FileDataSendRspMsg sendRsp;
		sendRsp.m_strMsgId = reqMsg.m_strMsgId;
		sendRsp.m_strUserId = reqMsg.m_strUserId;
		sendRsp.m_strFriendId = reqMsg.m_strFriendId;
		sendRsp.m_nFileId = reqMsg.m_nFileId;
		sendRsp.m_nDataTotalCount = reqMsg.m_nDataTotalCount;
		sendRsp.m_nDataIndex = reqMsg.m_nDataIndex;
		m_udpServer->sendMsg(sendPt,&sendRsp);
	}
}

/**
 * @brief 处理文件验证请求消息，直接转发到文件接收端
 * 
 * @param pSess 用户会话
 * @param req 验证请求消息
 */
void CChatServer::HandleFileVerifyReq(const std::shared_ptr<CServerSess>& pSess, const FileVerifyReqMsg& req)
{

	{
		auto item = m_UserSessVec.find(req.m_strUserId);
		if (item != m_UserSessVec.end())
		{
			item->second->SendMsg(&req);
		}
		else
		{
			FileVerifyRspMsg rspMsg;
			rspMsg.m_strMsgId = req.m_strMsgId;
			rspMsg.m_strFileName = req.m_strFileName;
			rspMsg.m_strUserId = req.m_strUserId;
			rspMsg.m_strFriendId = req.m_strFriendId;
			rspMsg.m_nFileId = req.m_nFileId;
			rspMsg.m_eErrCode = ERROR_CODE_TYPE::E_CODE_SUCCEED;
			pSess->SendMsg(&rspMsg);
		}
		m_fileUtil.OnCloseFile(req.m_nFileId);
	}

	{
		std::string strFileName = GetFilePathByUserIdAndFileName(req.m_strUserId, req.m_strFileName);
		std::string strFileHash = m_fileUtil.CalcHash(strFileName);
		if (strFileHash == req.m_strFileHash)
		{
			T_FILE_HASH_BEAN bean;
			bean.m_strF_FILE_HASH = strFileHash;
			bean.m_strF_FILE_NAME = req.m_strFileName;
			bean.m_strF_USER_ID = req.m_strUserId;
			m_util.InsertFileHash(bean);
		}
	}
}

/**
 * @brief 处理文件验证消息的回复,直接转发到文件发送者
 * 
 * @param pSess 用户会话
 * @param req 文件验证回复消息
 */
void CChatServer::HandleFileVerifyRsp(const std::shared_ptr<CServerSess>& pSess, const FileVerifyRspMsg& req)
{
	{
		auto item = m_UserSessVec.find(req.m_strFriendId);
		if (item != m_UserSessVec.end())
		{
			item->second->SendMsg(&req);
		}
	}
}


/**
 * @brief 处理收到的接收文件数据回复消息
 * 
 * TODO: 可能需要在此函数区分是好友数据还是群组数据
 * @param sendPt 消息发送者的地址
 * @param rspMsg 接收文件数据回复消息
 */
void CChatServer::Handle_RecvUdpMsg(const asio::ip::udp::endpoint sendPt, const FileDataRecvRspMsg& rspMsg)
{
	if(rspMsg.m_nDataIndex < rspMsg.m_nDataTotalCount)
	{
		{
			FileDataRecvReqMsg sendReqMsg;
			sendReqMsg.m_strMsgId = std::to_string(m_MsgID_Util.nextId());
			sendReqMsg.m_strUserId = rspMsg.m_strUserId;
			sendReqMsg.m_strFriendId = rspMsg.m_strFriendId;
			sendReqMsg.m_nFileId = rspMsg.m_nFileId;

			sendReqMsg.m_nDataTotalCount = rspMsg.m_nDataTotalCount;
			sendReqMsg.m_nDataIndex = rspMsg.m_nDataIndex+1;
			sendReqMsg.m_nDataLength = 0;
			if (m_fileUtil.OnReadData(sendReqMsg.m_nFileId, sendReqMsg.m_szData, sendReqMsg.m_nDataLength, 1024))
			{
				auto item = m_userIdUdpAddrMap.find(rspMsg.m_strUserId);
				if (item != m_userIdUdpAddrMap.end())
				{
					m_udpServer->sendMsg(item->second.m_strServerIp, item->second.m_nPort, &sendReqMsg);
				}
				else
				{
					LOG_ERR(ms_loger, "Can not find {} ", rspMsg.m_strUserId);
				}
			}
		}
	}
	else
	{
		std::string strFileName = m_fileUtil.GetFileName(rspMsg.m_nFileId);
		FileVerifyReqMsg reqMsg;
		reqMsg.m_nFileId = rspMsg.m_nFileId;
		reqMsg.m_strMsgId = CreateMsgId();
		reqMsg.m_strUserId = rspMsg.m_strUserId;
		reqMsg.m_strFriendId = rspMsg.m_strFriendId;
		reqMsg.m_strFileName = m_fileUtil.GetFileNameFromPath(strFileName);
		reqMsg.m_strFileHash = m_fileUtil.CalcHash(strFileName);
		m_fileUtil.GetFileSize(reqMsg.m_nFileSize,strFileName);
		
		auto item = m_UserSessVec.find(reqMsg.m_strUserId);
		if (item != m_UserSessVec.end() && item->second->IsConnected())
		{
			item->second->SendMsg(&reqMsg);
		}
	}
}



/**
 * @brief 实际响应加入群组请求
 * 
 * @param reqMsg 用户加入群组请求
 * @return AddToGroupRspMsg 用户加入群组回复
 */
AddToGroupRspMsg CChatServer::DoAddToGroupReqMsg(const AddToGroupReqMsg& reqMsg)
{
	AddToGroupRspMsg rspMsg;
	T_GROUP_RELATION_BEAN relationBean;
	relationBean.m_eRole = E_GROUP_MEMBER_ROLE::E_ROLE_MEMBER;
	relationBean.m_strF_GROUP_ID = reqMsg.m_strGroupId;
	relationBean.m_strF_USER_ID = reqMsg.m_strUserId;
	if (m_util.InsertGroupRelation(relationBean))
	{
		rspMsg.m_eErrCode = ERROR_CODE_TYPE::E_CODE_SUCCEED;
		rspMsg.m_strGroupId = reqMsg.m_strGroupId;
		rspMsg.m_strUserId = reqMsg.m_strUserId;
	}
	else
	{
		rspMsg.m_eErrCode = ERROR_CODE_TYPE::E_CODE_LOGIN_FAILED;
	}
	return rspMsg;
}


/**
 * @brief 分发群组聊天消息
 * 
 * @param strGroupId 群组ID
 */
void CChatServer::OnDispatchGroupMsg(const std::string strGroupId)
{
	std::vector<T_GROUP_RELATION_BEAN> groupUsers;
	if (m_util.SelectGroupRelation(strGroupId, groupUsers))
	{
		for (auto item : groupUsers)
		{
			OnUserRecvGroupMsg(item.m_strF_USER_ID);
		}
	}
}

/**
 * @brief 处理用户接收群组消息
 * 
 * @param strUser 用户ID
 * @param msg 群聊消息
 * @return true 成功
 * @return false 失败
 */
bool CChatServer::OnUserRecvGroupMsg(const std::string strUser, const T_GROUP_CHAT_MSG& msg)
{
	auto item = m_UserSessVec.find(strUser);
	if (item != m_UserSessVec.end())
	{
		if (CHAT_MSG_TYPE::E_CHAT_TEXT_TYPE == msg.m_eChatMsgType)
		{
			RecvGroupTextMsgReqMsg reqMsg;
			reqMsg.m_strGroupId = msg.m_strF_GROUP_ID;
			reqMsg.m_strUserId = strUser;
			reqMsg.m_strSenderId = msg.m_strF_SENDER_ID;
			reqMsg.m_strContext = msg.m_strF_MSG_CONTEXT;
			reqMsg.m_strMsgId = msg.m_strF_MSG_ID;
			reqMsg.m_stFontInfo.FromString(msg.m_strF_OTHER_INFO);
			reqMsg.m_strMsgTime = msg.m_strF_CREATE_TIME;
			item->second->SendMsg(&reqMsg);
			return true;
		}

	}
	return false;
}

/**
 * @brief 发送群聊天消息到用户
 * 
 * @param strUser 用户ID
 * @param strMsgId 消息ID
 * @param strGroupId 群组ID
 * @param strSender 消息发送者ID
 * @param strMsg 消息内容
 * @return true 发送成功
 * @return false 发送失败
 */
bool CChatServer::OnUserRecvGroupMsg(const std::string strUser,const std::string strMsgId, const std::string strGroupId, const std::string strSender, const std::string strMsg)
{
	auto item = m_UserSessVec.find(strUser);
	if (item != m_UserSessVec.end())
	{
		RecvGroupTextMsgReqMsg reqMsg;
		reqMsg.m_strGroupId = strGroupId;
		reqMsg.m_strSenderId = strSender;
		reqMsg.m_strContext = strMsg;
		reqMsg.m_strMsgId = strMsgId;
		item->second->SendMsg(&reqMsg);
		return true;
	}
	return false;
}

/**
 * @brief 处理用户发送的群组文本聊天消息请求
 * 
 * @param reqMsg 群组文字聊天消息请求
 * @return SendGroupTextMsgRspMsg 群组文字聊天消息回复
 */
SendGroupTextMsgRspMsg CChatServer::DoSendGroupTextMsgReqMsg(const SendGroupTextMsgReqMsg& reqMsg)
{
	SendGroupTextMsgRspMsg rspMsg;
	T_GROUP_CHAT_MSG chatMsg;
	chatMsg.m_strF_GROUP_ID = reqMsg.m_strGroupId;
	chatMsg.m_strF_SENDER_ID = reqMsg.m_strSenderId;
	chatMsg.m_strF_MSG_CONTEXT = reqMsg.m_strContext;
	chatMsg.m_strF_MSG_ID = std::to_string(m_MsgID_Util.nextId());
	chatMsg.m_eChatMsgType = CHAT_MSG_TYPE::E_CHAT_TEXT_TYPE;
	chatMsg.m_strF_OTHER_INFO = reqMsg.m_stFontInfo.ToString();
	if (m_util.InsertGroupChatText(chatMsg))
	{
		rspMsg.m_eErrCode = ERROR_CODE_TYPE::E_CODE_SUCCEED;
		rspMsg.m_strSenderId = reqMsg.m_strSenderId;
		rspMsg.m_strGroupId = reqMsg.m_strGroupId;
		rspMsg.m_strMsgId = chatMsg.m_strF_MSG_ID;
		rspMsg.m_strContext = reqMsg.m_strContext;
		rspMsg.m_fontInfo = reqMsg.m_stFontInfo;
		rspMsg.m_strMsgTime = CTimeUtil::GetYMD_HMS_Time();
	}
	else
	{
		rspMsg.m_eErrCode = ERROR_CODE_TYPE::E_CODE_CONNECT_FAILED;
	}

	return rspMsg;
}


/**
 * @brief 处理下发群聊文本消息后，客户端的回复
 * 
 * @param pSess 用户会话
 * @param reqMsg 下发群聊文本消息回复
 */
void CChatServer::HandleRecvGroupTextMsgRspMsg(const std::shared_ptr<CServerSess>& pSess, const RecvGroupTextMsgRspMsg& reqMsg)
{
	T_GROUP_RELATION_BEAN relationBean;
	relationBean.m_strF_GROUP_ID = reqMsg.m_strGroupId;
	relationBean.m_strF_USER_ID = pSess->UserId();
	relationBean.m_strF_LAST_READ_MSG_ID = reqMsg.m_strMsgId;
	
	if (m_util.UpdateGroupRelationLastReadId(relationBean))
	{

	}


	T_GROUP_CHAT_MSG msgBean;
	msgBean.m_strF_GROUP_ID = reqMsg.m_strGroupId;
	msgBean.m_strF_MSG_ID = reqMsg.m_strMsgId;
	if (m_util.SelectGroupChatText(msgBean))
	{
		if (CHAT_MSG_TYPE::E_CHAT_TEXT_TYPE == msgBean.m_eChatMsgType)
		{
			OnUserRecvGroupMsg(pSess->UserId(), msgBean);
			//OnUserRecvGroupMsg(reqMsg.m_strSenderId, msgBean.m_strF_MSG_ID, msgBean.m_strF_GROUP_ID, msgBean.m_strF_SENDER_ID, msgBean.m_strF_MSG_CONTEXT);
		}
	}
}

/**
 * @brief 处理用户重复登陆的问题
 * 
 * @param strUserId 用户ID
 * @param pSess 用户会话
 */
void CChatServer::HandleReLogin(std::string strUserId, std::shared_ptr<CServerSess> pSess)
{
	auto item = m_UserSessVec.find(strUserId);
	if (item != m_UserSessVec.end())
	{
		//同一个Socket发起两次登录
		if (item->second == pSess)
		{
			LOG_INFO(ms_loger, "User:{} Login From {} Twice", strUserId, pSess->GetRemoteIp());
			return;
		}
		else
		{
			pSess->SetUserId(strUserId);
			m_UserSessVec.insert(std::pair<std::string, std::shared_ptr<CServerSess>>(strUserId, pSess));
			LOG_WARN(ms_loger, "User:{} ReLogin From {} OldLoginIp:{} ", strUserId, pSess->GetRemoteIp(), item->second->GetRemoteIp());
			auto pOldSess = item->second;
			m_KickOffSessMap.insert({ strUserId,item->second });
			m_UserSessVec.erase(strUserId);
			m_clientStateMap.erase(strUserId);
			{
				UserKickOffReqMsg reqMsg;
				reqMsg.m_strMsgId = std::to_string(m_MsgID_Util.nextId());
				reqMsg.m_strUserId = strUserId;
				reqMsg.m_strNewLoginIp = pSess->GetRemoteIp();

				pOldSess->SendMsg(&reqMsg);
			}
		}
	}
	else
	{
		pSess->SetUserId(strUserId);
		m_UserSessVec.insert(std::pair<std::string, std::shared_ptr<CServerSess>>(strUserId, pSess));
		m_clientStateMap.insert({ strUserId,CLIENT_SESS_STATE::SESS_IDLE_STATE });
	}
}

/**
 * @brief 响应用户被踢掉(其他地方登陆)的消息回复
 * 
 * @param pSess 用户会话
 * @param reqMsg KickOff消息回复
 */
void CChatServer::HandleUserKickOffRsp(const std::shared_ptr<CServerSess>& pSess, const UserKickOffRspMsg& reqMsg)
{
	auto item = m_KickOffSessMap.find(reqMsg.m_strUserId);
	item->second->CloseSocket();
	if (item != m_KickOffSessMap.end())
	{
		LOG_INFO(ms_loger, "User:{} KickOff Finished ", reqMsg.m_strUserId);
		m_KickOffSessMap.erase(reqMsg.m_strUserId);
	}
	else
	{
		LOG_ERR(ms_loger, "User:{} Should Not Receive KickOff ", reqMsg.m_strUserId);
	}
}

std::string CChatServer::CreateMsgId()
{
	return std::to_string(m_MsgID_Util.nextId());
}
/**
 * @brief 生成密码需要的Salt
 * 
 * @return std::string Salt字符串
 */
std::string CChatServer::GenerateSalt()
{
	const char buff[] = "0123456789ABCDEFGHIGKLMNOPQRSTUVWXYZabcdefghigklmnopqrstuvwxyz";
	constexpr int bufLen = sizeof(buff) / sizeof(buff[0]);
	std::string strResult;
	for (int i = 0; i < HASH_SALT_LENGTH; i++)
	{
		strResult += buff[rand() % bufLen];
	}

	return strResult;
}

/**
 * @brief 从数据库的密码中提取Salt
 * 
 * @param strPasswd 数据库的密码
 * @return std::string 对应的Salt
 */
std::string CChatServer::GetSaltFromPasswd(const std::string strPasswd)
{
	if (strPasswd.length() > HASH_SALT_LENGTH)
	{
		return strPasswd.substr(0, HASH_SALT_LENGTH);
	}
	return "";
}

/**
 * @brief 根据原始密码生成密码
 * 
 * @param orgPassWord 原始密码
 * @return std::string 新的密码
 */
std::string CChatServer::GeneratePassword(const std::string orgPassWord)
{
	std::string strSalt = GenerateSalt();
	std::string strOrg = strSalt + orgPassWord;
	std::string strOrgHash = md5(strOrg);
	return strSalt + strOrgHash;
}

/**
 * @brief 校验密码
 * 
 * @param orgPassword 原始密码
 * @param dataBasePassword 数据库的密码
 * @return true 校验通过
 * @return false 校验失败
 */
bool CChatServer::VerifyPassword(const std::string orgPassword, const std::string dataBasePassword)
{
	std::string strSalt = GetSaltFromPasswd(dataBasePassword);
	std::string strPasswd = strSalt+md5(strSalt + orgPassword);
	if (strPasswd == dataBasePassword)
	{
		return true;
	}
	else
	{
		return false;
	}
}

/**
 * @brief 根据用户ID和文件名，拼接出文件全路径
 * 
 * @param strUserId 用户ID
 * @param strFileName 文件名
 * @return std::string 文件全路径
 */
std::string CChatServer::GetFilePathByUserIdAndFileName(const std::string strUserId, const std::string strFileName)
{
	return GetFolderByUserId(strUserId) + strFileName;
}

/**
 * @brief 根据用户ID获取对应的文件夹
 * 
 * @param strUserId 用户ID
 * @return std::string 用户ID对应的文件夹
 */
std::string CChatServer::GetFolderByUserId(const std::string strUserId)
{
	std::string strFolder = m_fileUtil.GetCurDir() + "\\" + strUserId+"\\";
	return strFolder;
}

}