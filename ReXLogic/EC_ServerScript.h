// For conditions of distribution and use, see copyright notice in license.txt

#ifndef incl_EC_ServerScript_h
#define incl_EC_ServerScript_h

#include "ComponentInterface.h"
#include "Foundation.h"


class EC_ServerScript : public Foundation::ComponentInterface
{
    DECLARE_EC(EC_ServerScript);
public:
    virtual ~EC_ServerScript();

    static std::vector<std::string> GetNetworkMessages()
    {
        std::vector<std::string> myinterest;
        myinterest.push_back("GeneralMessage_ExtraEntityData");
        return myinterest;
    } 

private:
    EC_ServerScript();

    std::string class_name_;
};

#endif
