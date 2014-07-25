#ifndef MPTCP_CC_OLIA_H
#define MPTCP_CC_OLIA_H

#include"ns3/mp-tcp-cc.h"
#include"ns3/mp-tcp-subflow.h"
#include"ns3/mp-tcp-socket-base.h"
#include"ns3/callback.h"

namespace ns3
{


/**
This is the linux struct.  We may try to get sthg close to this
http://www.nsnam.org/wiki/New_TCP_Socket_Architecture#Pluggable_Congestion_Control_in_Linux_TCP
static struct tcp_congestion_ops mptcp_olia = {
	.init		= mptcp_olia_init,
	.ssthresh	= tcp_reno_ssthresh,
	.cong_avoid	= mptcp_olia_cong_avoid,
	.set_state	= mptcp_olia_set_state,
	.min_cwnd	= tcp_reno_min_cwnd,
};
**/
class MpTcpCCOlia : public MpTcpCongestionControl
{

public:

  MpTcpCCOlia();
  virtual ~MpTcpCCOlia();

  virtual uint32_t
  GetSSThresh(void) const;

  virtual uint32_t
  GetInitialCwnd(void) const;

  // transform into a callback ?
  // Callback<Ptr<MpTcpSubFlow>, Ptr<MpTcpSocketBase>, Ptr<MpTcpCongestionControl> >
  //Ptr<MpTcpSubFlow>

  // Called by SendPendingData() to get a subflow based on round robin algorithm
  virtual int GeneratePartition(Ptr<MpTcpSocketBase> metaSock);

  virtual const char*
  GetName(void) const {
    return "OLIA";
  };

//  SendPendingData()
};


}


#endif /* MPTCP_CC_OLIA_H */
