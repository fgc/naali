// For conditions of distribution and use, see copyright notice in license.txt

#ifndef __incl_EC_SpatialSound_h__
#define __incl_EC_SpatialSound_h__

#include "ComponentInterface.h"
#include "Foundation.h"


class EC_SpatialSound : public Foundation::ComponentInterface
{
    DECLARE_EC(EC_SpatialSound);
public:
    virtual ~EC_SpatialSound();


    virtual void handleNetworkData(std::string data);

    static std::vector<std::string> getNetworkMessages()
    {
        std::vector<std::string> myinterest;
        myinterest.push_back("GeneralMessage_ExtraEntityData");
        return myinterest;
    } 

private:
    EC_SpatialSound();

    std::string mSoundId;
    float mVolume;
    float mRadius;
};

#endif