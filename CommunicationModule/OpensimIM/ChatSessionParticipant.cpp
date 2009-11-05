#include "ChatSessionParticipant.h"
#include "../../ReXLogic/RexLogicModule.h" 

namespace OpensimIM
{
	ChatSessionParticipant::ChatSessionParticipant(const QString &uuid, const QString &name): uuid_(uuid), name_(name)
	{

	}

	ChatSessionParticipant::ChatSessionParticipant(Contact *contact): contact_(contact)
	{

	}

	Communication::ContactInterface* ChatSessionParticipant::GetContact() const
	{
		return contact_;
	}

	QString ChatSessionParticipant::GetID() const
	{
		return uuid_;
	}
	
	QString ChatSessionParticipant::GetName() const
	{
		return name_;
	}

	void ChatSessionParticipant::SetName(const QString &name)
	{
		name_ = name;
	}

} // end of namespace: OpensimIM
