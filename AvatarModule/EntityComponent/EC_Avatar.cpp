// For conditions of distribution and use, see copyright notice in license.txt

#include "StableHeaders.h"
#include "DebugOperatorNew.h"
#include "EntityComponent/EC_Avatar.h"
#include "EventManager.h"
#ifdef ENABLE_TAIGA_SUPPORT
#include "RexTypes.h"
#endif

#include "EC_Mesh.h"
#include "EC_AnimationController.h"
#include "EC_Placeable.h"
#include "EC_AvatarAppearance.h"
#include "AssetAPI.h"
#include "IAssetTransfer.h"
#include "AvatarDescAsset.h"
#include "Entity.h"
#include "OgreConversionUtils.h"
#include "../Avatar/LegacyAvatarSerializer.h"

#include <Ogre.h>
#include <QDomDocument>

#include "LoggingFunctions.h"
//DEFINE_POCO_LOGGING_FUNCTIONS("EC_Avatar")

#include "MemoryLeakCheck.h"

#ifdef ENABLE_TAIGA_SUPPORT
using namespace RexTypes;
#endif
using namespace Avatar;

// Internal helper functions
void SetupAppearance(Scene::Entity* entity);
void SetupDynamicAppearance(Scene::Entity* entity);
void AdjustHeightOffset(Scene::Entity* entity);
void SetupMeshAndMaterials(Scene::Entity* entity);
void SetupMorphs(Scene::Entity* entity);
void SetupBoneModifiers(Scene::Entity* entity);
void SetupAttachments(Scene::Entity* entity);
void ResetBones(Scene::Entity* entity);
void ApplyBoneModifier(Scene::Entity* entity, const BoneModifier& modifier, float value);
void HideVertices(Ogre::Entity*, std::set<uint> vertices_to_hide);
Ogre::Bone* GetAvatarBone(Scene::Entity* entity, const std::string& bone_name);
void GetInitialDerivedBonePosition(Ogre::Node* bone, Ogre::Vector3& position);
std::string LookupAsset(const AvatarAsset& asset, const AvatarAssetMap& asset_map);
std::string LookupMaterial(const AvatarMaterial& material, const AvatarAssetMap& asset_map);

// Regrettable magic value
static const float FIXED_HEIGHT_OFFSET = -0.87f;

EC_Avatar::EC_Avatar(IModule* module) :
    IComponent(module->GetFramework()),
    appearanceId(this, "Appearance ref", "")
{
    EventManager *event_manager = framework_->GetEventManager().get();
    if(event_manager)
    {
        event_manager->RegisterEventSubscriber(this, 99);
        asset_event_category_ = event_manager->QueryEventCategory("Asset");
    }
    else
    {
        LogWarning("Event manager was not valid.");
    }
    
    connect(this, SIGNAL(OnAttributeChanged(IAttribute*, AttributeChange::Type)),
        this, SLOT(AttributeUpdated(IAttribute*)));
}

EC_Avatar::~EC_Avatar()
{
}

void EC_Avatar::OnAvatarAppearanceLoaded(AssetPtr asset)
{
    if (!asset)
        return;

    Scene::Entity* entity = GetParentEntity();
    if (!entity)
        return;

    AvatarDescAssetPtr avatarAsset = boost::dynamic_pointer_cast<AvatarDescAsset>(asset);
    if (!avatarAsset)
        return;

    // Create components the avatar needs, with network sync disabled, if they don't exist yet
    // Note: the mesh & avatarappearance are created as non-syncable on purpose, as each client's EC_Avatar should execute this code upon receiving the appearance
    ComponentPtr mesh = entity->GetOrCreateComponent(EC_Mesh::TypeNameStatic(), AttributeChange::LocalOnly, false);
    entity->GetOrCreateComponent(EC_AvatarAppearance::TypeNameStatic(), AttributeChange::LocalOnly, false);
    EC_Mesh *mesh_ptr = checked_static_cast<EC_Mesh*>(mesh.get());
    // Attach to placeable if not yet attached
    if (mesh_ptr && !mesh_ptr->GetPlaceable())
        mesh_ptr->SetPlaceable(entity->GetComponent(EC_Placeable::TypeNameStatic()));
    
    SetupAvatar(avatarAsset);
}

void EC_Avatar::AttributeUpdated(IAttribute *attribute)
{
    if (attribute == &appearanceId)
    {
        QString ref = appearanceId.Get().trimmed();
        if (ref.isEmpty())
            return;

        AssetTransferPtr transfer = GetFramework()->Asset()->RequestAsset(ref.toStdString().c_str(), "GenericAvatarXml");
        if (transfer)
            connect(transfer.get(), SIGNAL(Loaded(AssetPtr)), this, SLOT(OnAvatarAppearanceLoaded(AssetPtr)));
    }
}

void EC_Avatar::SetupAvatar(AvatarDescAssetPtr avatarAsset)
{
    if (!avatarAsset)
        return;
    Scene::Entity* entity = GetParentEntity();
    if (!entity)
        return;
    EC_AvatarAppearance* appearance = entity->GetComponent<EC_AvatarAppearance>().get();
    if (!appearance)
        return;
    
    QDomDocument avatar_doc("Avatar");
    avatar_doc.setContent(avatarAsset->avatarAppearanceXML);
    
    // Deserialize appearance from the document into the EC
    if (!LegacyAvatarSerializer::ReadAvatarAppearance(*appearance, avatar_doc))
    {
        LogError("Failed to parse avatar description");
        return;
    }
    
    SetupAppearance(entity);
}

void SetupAppearance(Scene::Entity* entity)
{
    PROFILE(Avatar_SetupAppearance);
    
    if (!entity)
        return;
    
    EC_AvatarAppearance* appearance = entity->GetComponent<EC_AvatarAppearance>().get();
    EC_Mesh* mesh = entity->GetComponent<EC_Mesh>().get();

    if (!mesh || !appearance)
        return;

    // If mesh name is empty, it would certainly be an epic fail. Do nothing.
    if (appearance->GetMesh().name_.empty())
        return;
    
    // Setup appearance
    SetupMeshAndMaterials(entity);
    SetupDynamicAppearance(entity);
    SetupAttachments(entity);
}

void SetupDynamicAppearance(Scene::Entity* entity)
{
    if (!entity)
        return;
    
    EC_AvatarAppearance* appearance = entity->GetComponent<EC_AvatarAppearance>().get();
    EC_Mesh* mesh = entity->GetComponent<EC_Mesh>().get();

    if (!mesh || !appearance)
        return;
    
    SetupMorphs(entity);
    SetupBoneModifiers(entity);
    AdjustHeightOffset(entity);
}

void AdjustHeightOffset(Scene::Entity* entity)
{
    if (!entity)
        return;
    
    EC_AvatarAppearance* appearance = entity->GetComponent<EC_AvatarAppearance>().get();
    EC_Mesh* mesh = entity->GetComponent<EC_Mesh>().get();

    if (!mesh || !appearance)
        return;
    
    Ogre::Vector3 offset = Ogre::Vector3::ZERO;
    Ogre::Vector3 initial_base_pos = Ogre::Vector3::ZERO;

    if (appearance->HasProperty("baseoffset"))
    {
        initial_base_pos = Ogre::StringConverter::parseVector3(appearance->GetProperty("baseoffset"));
    }

    if (appearance->HasProperty("basebone"))
    {
        Ogre::Bone* base_bone = GetAvatarBone(entity, appearance->GetProperty("basebone"));
        if (base_bone)
        {
            Ogre::Vector3 temp;
            GetInitialDerivedBonePosition(base_bone, temp);
            initial_base_pos += temp;
            offset = initial_base_pos;

            // Additionally, if has the rootbone property, can do dynamic adjustment for sitting etc.
            // and adjust the name overlay height
            if (appearance->HasProperty("rootbone"))
            {
                Ogre::Bone* root_bone = GetAvatarBone(entity, appearance->GetProperty("rootbone"));
                if (root_bone)
                {
                    Ogre::Vector3 initial_root_pos;
                    Ogre::Vector3 current_root_pos = root_bone->_getDerivedPosition();
                    GetInitialDerivedBonePosition(root_bone, initial_root_pos);

                    float c = abs(current_root_pos.y / initial_root_pos.y);
                    if (c > 1.0) c = 1.0;
                    offset = initial_base_pos * c;

                }
            }
        }
    }

    mesh->SetAdjustPosition(Vector3df(0.0f, 0.0f, -offset.y + FIXED_HEIGHT_OFFSET));
}

void SetupMeshAndMaterials(Scene::Entity* entity)
{
    EC_AvatarAppearance* appearance = entity->GetComponent<EC_AvatarAppearance>().get();
    EC_Mesh* mesh = entity->GetComponent<EC_Mesh>().get();
    const AvatarAssetMap& assetMap = appearance->GetAssetMap();
    
    // Mesh needs to be cloned if there are attachments which need to hide vertices
    bool need_mesh_clone = false;
    
    const AvatarAttachmentVector& attachments = appearance->GetAttachments();
    std::set<uint> vertices_to_hide;
    for(uint i = 0; i < attachments.size(); ++i)
    {
        if (attachments[i].vertices_to_hide_.size())
        {
            need_mesh_clone = true;
            for(uint j = 0; j < attachments[i].vertices_to_hide_.size(); ++j)
                vertices_to_hide.insert(attachments[i].vertices_to_hide_[j]);
        }
    }
    
    std::string meshName = LookupAsset(appearance->GetMesh(), assetMap);
    std::string skeletonName = LookupAsset(appearance->GetSkeleton(), assetMap);
    // If skeleton name is local, ie. not an asset, do not set it explicitly
    if (assetMap.find(appearance->GetSkeleton().name_) == assetMap.end())
        skeletonName = "";
    
    if (!skeletonName.empty())
        mesh->SetMeshWithSkeleton(meshName, skeletonName, need_mesh_clone);
    else
        mesh->SetMesh(QString::fromStdString(meshName), need_mesh_clone);
    
    if (need_mesh_clone)
        HideVertices(mesh->GetEntity(), vertices_to_hide);
    
    const AvatarMaterialVector& materials = appearance->GetMaterials();
    
    for(uint i = 0; i < materials.size(); ++i)
    {
        mesh->SetMaterial(i, LookupMaterial(materials[i], assetMap));
    }
    
    // Set adjustment orientation for mesh (Ogre meshes usually have Y-axis as vertical)
    Quaternion adjust(PI/2, 0, -PI/2);
    mesh->SetAdjustOrientation(adjust);
    // Position approximately within the bounding box
    // Will be overridden by bone-based height adjust, if available
    mesh->SetAdjustPosition(Vector3df(0.0f, 0.0f, FIXED_HEIGHT_OFFSET));
    mesh->SetCastShadows(true);
}

void SetupAttachments(Scene::Entity* entity)
{
    EC_AvatarAppearance* appearance = entity->GetComponent<EC_AvatarAppearance>().get();
    EC_Mesh* mesh = entity->GetComponent<EC_Mesh>().get();
    const AvatarAssetMap& assetMap = appearance->GetAssetMap();
    
    mesh->RemoveAllAttachments();
    
    const AvatarAttachmentVector& attachments = appearance->GetAttachments();
    
    for(uint i = 0; i < attachments.size(); ++i)
    {
        // Setup attachment meshes
        mesh->SetAttachmentMesh(i, LookupAsset(attachments[i].mesh_, assetMap), attachments[i].bone_name_, attachments[i].link_skeleton_);
        // Setup attachment mesh materials
        for(uint j = 0; j < attachments[i].materials_.size(); ++j)
        {
            mesh->SetAttachmentMaterial(i, j, LookupMaterial(attachments[i].materials_[j], assetMap));
        }
        mesh->SetAttachmentPosition(i, attachments[i].transform_.position_);
        mesh->SetAttachmentOrientation(i, attachments[i].transform_.orientation_);
        mesh->SetAttachmentScale(i, attachments[i].transform_.scale_);
    }
}

void SetupMorphs(Scene::Entity* entity)
{
    EC_AvatarAppearance* appearance = entity->GetComponent<EC_AvatarAppearance>().get();
    EC_Mesh* mesh = entity->GetComponent<EC_Mesh>().get();
            
    Ogre::Entity* ogre_entity = mesh->GetEntity();
    if (!ogre_entity)
        return;
    Ogre::AnimationStateSet* anims = ogre_entity->getAllAnimationStates();
    if (!anims)
        return;
        
    const MorphModifierVector& morphs = appearance->GetMorphModifiers();
    for(uint i = 0; i < morphs.size(); ++i)
    {
        if (anims->hasAnimationState(morphs[i].morph_name_))
        {
            float timePos = morphs[i].value_;
            if (timePos < 0.0f)
                timePos = 0.0f;
            // Clamp very close to 1.0, but do not actually go to 1.0 or the morph animation will wrap
            if (timePos > 0.99995f)
                timePos = 0.99995f;
            
            Ogre::AnimationState* anim = anims->getAnimationState(morphs[i].morph_name_);
            anim->setTimePosition(timePos);
            anim->setEnabled(timePos > 0.0f);
            
            // Also set position in attachment entities, if have the same morph
            for(uint j = 0; j < mesh->GetNumAttachments(); ++j)
            {
                Ogre::Entity* attachment = mesh->GetAttachmentEntity(j);
                if (!attachment)
                    continue;
                Ogre::AnimationStateSet* attachment_anims = attachment->getAllAnimationStates();
                if (!attachment_anims)
                    continue;
                if (!attachment_anims->hasAnimationState(morphs[i].morph_name_))
                    continue;
                Ogre::AnimationState* attachment_anim = attachment_anims->getAnimationState(morphs[i].morph_name_);
                attachment_anim->setTimePosition(timePos);
                attachment_anim->setEnabled(timePos > 0.0f);
            }
        }
    }
}

void SetupBoneModifiers(Scene::Entity* entity)
{
    EC_AvatarAppearance* appearance = entity->GetComponent<EC_AvatarAppearance>().get();
            
    ResetBones(entity);
    
    const BoneModifierSetVector& bone_modifiers = appearance->GetBoneModifiers();
    for(uint i = 0; i < bone_modifiers.size(); ++i)
    {
        for(uint j = 0; j < bone_modifiers[i].modifiers_.size(); ++j)
        {
            ApplyBoneModifier(entity, bone_modifiers[i].modifiers_[j], bone_modifiers[i].value_);
        }
    }
}

void ResetBones(Scene::Entity* entity)
{
    EC_AvatarAppearance* appearance = entity->GetComponent<EC_AvatarAppearance>().get();
    UNREFERENCED_PARAM(appearance);
    EC_Mesh* mesh = entity->GetComponent<EC_Mesh>().get();

    Ogre::Entity* ogre_entity = mesh->GetEntity();
    if (!ogre_entity)
        return;
    // See that we actually have a skeleton
    Ogre::SkeletonInstance* skeleton = ogre_entity->getSkeleton();
    Ogre::Skeleton* orig_skeleton = ogre_entity->getMesh()->getSkeleton().get();
    if ((!skeleton) || (!orig_skeleton))
        return;
    
    if (skeleton->getNumBones() != orig_skeleton->getNumBones())
        return;
    
    for(uint i = 0; i < orig_skeleton->getNumBones(); ++i)
    {
        Ogre::Bone* bone = skeleton->getBone(i);
        Ogre::Bone* orig_bone = orig_skeleton->getBone(i);

        bone->setPosition(orig_bone->getInitialPosition());
        bone->setOrientation(orig_bone->getInitialOrientation());
        bone->setScale(orig_bone->getInitialScale());
        bone->setInitialState();
    }
}

void ApplyBoneModifier(Scene::Entity* entity, const BoneModifier& modifier, float value)
{
    EC_Mesh* mesh = entity->GetComponent<EC_Mesh>().get();
    
    Ogre::Entity* ogre_entity = mesh->GetEntity();
    if (!ogre_entity)
        return;
    // See that we actually have a skeleton
    Ogre::SkeletonInstance* skeleton = ogre_entity->getSkeleton();
    Ogre::Skeleton* orig_skeleton = ogre_entity->getMesh()->getSkeleton().get();
    if ((!skeleton) || (!orig_skeleton))
        return;
    
    if ((!skeleton->hasBone(modifier.bone_name_)) || (!orig_skeleton->hasBone(modifier.bone_name_)))
        return; // Bone not found, nothing to do
        
    Ogre::Bone* bone = skeleton->getBone(modifier.bone_name_);
    Ogre::Bone* orig_bone = orig_skeleton->getBone(modifier.bone_name_);
    
    if (value < 0.0f)
        value = 0.0f;
    if (value > 1.0f)
        value = 1.0f;
    
    // Rotation
    {
        Ogre::Matrix3 rot_start, rot_end, rot_base, rot_orig;
        Ogre::Radian sx, sy, sz;
        Ogre::Radian ex, ey, ez;
        Ogre::Radian bx, by, bz;
        Ogre::Radian rx, ry, rz;
        OgreRenderer::ToOgreQuaternion(modifier.start_.orientation_).ToRotationMatrix(rot_start);
        OgreRenderer::ToOgreQuaternion(modifier.end_.orientation_).ToRotationMatrix(rot_end);
        bone->getInitialOrientation().ToRotationMatrix(rot_orig);
        rot_start.ToEulerAnglesXYZ(sx, sy, sz);
        rot_end.ToEulerAnglesXYZ(ex, ey, ez);
        rot_orig.ToEulerAnglesXYZ(rx, ry, rz);
        
        switch (modifier.orientation_mode_)
        {
        case BoneModifier::Absolute:
            bx = 0;
            by = 0;
            bz = 0;
            break;
            
        case BoneModifier::Relative:
            orig_bone->getInitialOrientation().ToRotationMatrix(rot_base);
            rot_base.ToEulerAnglesXYZ(bx, by, bz);
            break;
            
        case BoneModifier::Cumulative:
            bone->getInitialOrientation().ToRotationMatrix(rot_base);
            rot_base.ToEulerAnglesXYZ(bx, by, bz);
            break;
        }
        
        if (sx != Ogre::Radian(0) || ex != Ogre::Radian(0))
            rx = bx + sx * (1.0 - value) + ex * (value);
        if (sy != Ogre::Radian(0) || ey != Ogre::Radian(0))
            ry = by + sy * (1.0 - value) + ey * (value);
        if (sz != Ogre::Radian(0) || ez != Ogre::Radian(0))
            rz = bz + sz * (1.0 - value) + ez * (value);
        
        Ogre::Matrix3 rot_new;
        rot_new.FromEulerAnglesXYZ(rx, ry, rz);
        Ogre::Quaternion q_new(rot_new);
        bone->setOrientation(Ogre::Quaternion(rot_new));
    }
    
    // Translation
    {
        float sx = modifier.start_.position_.x;
        float sy = modifier.start_.position_.y;
        float sz = modifier.start_.position_.z;
        float ex = modifier.end_.position_.x;
        float ey = modifier.end_.position_.y;
        float ez = modifier.end_.position_.z;
        
        Ogre::Vector3 trans, base;
        trans = bone->getInitialPosition();
        switch (modifier.position_mode_)
        {
        case BoneModifier::Absolute:
            base = Ogre::Vector3(0,0,0);
            break;
        case BoneModifier::Relative:
            base = orig_bone->getInitialPosition();
            break;
        }
        
        if (sx != 0 || ex != 0)
            trans.x = base.x + sx * (1.0 - value) + ex * value;
        if (sy != 0 || ey != 0)
            trans.y = base.y + sy * (1.0 - value) + ey * value;
        if (sz != 0 || ez != 0)
            trans.z = base.z + sz * (1.0 - value) + ez * value;
        
        bone->setPosition(trans);
    }
    
    // Scale
    {
        Ogre::Vector3 scale = bone->getInitialScale();
        float sx = modifier.start_.scale_.x;
        float sy = modifier.start_.scale_.y;
        float sz = modifier.start_.scale_.z;
        float ex = modifier.end_.scale_.x;
        float ey = modifier.end_.scale_.y;
        float ez = modifier.end_.scale_.z;
        
        if (sx != 1 || ex != 1)
            scale.x = sx * (1.0 - value) + ex * value;
        if (sy != 1 || ey != 1)
            scale.y = sy * (1.0 - value) + ey * value;
        if (sz != 1 || ez != 1)
            scale.z = sz * (1.0 - value) + ez * value;
        
        bone->setScale(scale);
    }
    
    bone->setInitialState();
}

void GetInitialDerivedBonePosition(Ogre::Node* bone, Ogre::Vector3& position)
{
    // Hacky and slow way to derive the initial position of the base bone. Do not use current position
    // because animations change it
    position = bone->getInitialPosition();
    Ogre::Vector3 scale = bone->getInitialScale();
    Ogre::Quaternion orient = bone->getInitialOrientation();

    while(bone->getParent())
    {
       Ogre::Node* parent = bone->getParent();

       if (bone->getInheritOrientation())
       {
          orient = parent->getInitialOrientation() * orient;
       }
       if (bone->getInheritScale())
       {
          scale = parent->getInitialScale() * scale;
       }

       position = parent->getInitialOrientation() * (parent->getInitialScale() * position);
       position += parent->getInitialPosition();

       bone = parent;
    }
}

Ogre::Bone* GetAvatarBone(Scene::Entity* entity, const std::string& bone_name)
{
    if (!entity)
        return 0;            
    EC_Mesh* mesh = entity->GetComponent<EC_Mesh>().get();
    if (!mesh)
        return 0;
    
    Ogre::Entity* ogre_entity = mesh->GetEntity();
    if (!ogre_entity)
        return 0;
    Ogre::SkeletonInstance* skeleton = ogre_entity->getSkeleton();
    if (!skeleton)
        return 0;
    if (!skeleton->hasBone(bone_name))
        return 0;
    return skeleton->getBone(bone_name);
}

void HideVertices(Ogre::Entity* entity, std::set<uint> vertices_to_hide)
{
    if (!entity)
        return;
    Ogre::MeshPtr mesh = entity->getMesh();
    if (mesh.isNull())
        return;
    if (!mesh->getNumSubMeshes())
        return;
    for(uint m = 0; m < 1; ++m)
    {
        // Under current system, it seems vertices should only be hidden from first submesh
        Ogre::SubMesh *submesh = mesh->getSubMesh(m);
        if (!submesh)
            return;
        Ogre::IndexData *data = submesh->indexData;
        if (!data)
            return;
        Ogre::HardwareIndexBufferSharedPtr ibuf = data->indexBuffer;
        if (ibuf.isNull())
            return;

        unsigned long* lIdx = static_cast<unsigned long*>(ibuf->lock(Ogre::HardwareBuffer::HBL_NORMAL));
        unsigned short* pIdx = reinterpret_cast<unsigned short*>(lIdx);
        bool use32bitindexes = (ibuf->getType() == Ogre::HardwareIndexBuffer::IT_32BIT);

        for(uint n = 0; n < data->indexCount; n += 3)
        {
            if (!use32bitindexes)
            {
                if (vertices_to_hide.find(pIdx[n]) != vertices_to_hide.end() ||
                    vertices_to_hide.find(pIdx[n+1]) != vertices_to_hide.end() ||
                    vertices_to_hide.find(pIdx[n+2]) != vertices_to_hide.end())
                {
                    if (n + 3 < data->indexCount)
                    {
                        for(size_t i = n ; i<data->indexCount-3 ; ++i)
                        {
                            pIdx[i] = pIdx[i+3];
                        }
                    }
                    data->indexCount -= 3;
                    n -= 3;
                }
            }
            else
            {
                if (vertices_to_hide.find(lIdx[n]) != vertices_to_hide.end() ||
                    vertices_to_hide.find(lIdx[n+1]) != vertices_to_hide.end() ||
                    vertices_to_hide.find(lIdx[n+2]) != vertices_to_hide.end())
                {
                    if (n + 3 < data->indexCount)
                    {
                        for(size_t i = n ; i<data->indexCount-3 ; ++i)
                        {
                            lIdx[i] = lIdx[i+3];
                        }
                    }
                    data->indexCount -= 3;
                    n -= 3;
                }
            }
        }
        ibuf->unlock();
    }
}

std::string LookupAsset(const AvatarAsset& asset, const AvatarAssetMap& asset_map)
{
    AvatarAssetMap::const_iterator i = asset_map.find(asset.name_);
    if (i != asset_map.end())
        return i->second;
    else
        return asset.name_;
}

std::string LookupMaterial(const AvatarMaterial& material, const AvatarAssetMap& asset_map)
{
    std::string matName = material.asset_.name_;
    
    AvatarAssetMap::const_iterator i = asset_map.find(matName);
    if (i != asset_map.end())
        return i->second;
    // If asset name had no .material in it, append and try again
    if (matName.find(".material") == std::string::npos)
    {
        matName.append(".material");
        i = asset_map.find(matName);
        if (i != asset_map.end())
            return i->second;
    }
    
    // If no match in assetmap, just return the original name
    return material.asset_.name_;
}
