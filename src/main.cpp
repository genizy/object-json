#include "Geode/ui/Popup.hpp"
#include "Geode/utils/file.hpp"
#include "json.hpp"

#include <Geode/binding/ButtonSprite.hpp>
#include <Geode/binding/EditorUI.hpp>
#include <fstream>
#include <Geode/modify/EditorUI.hpp>
#include <Geode/Geode.hpp>
#include <unordered_set>

using namespace cocos2d;
using json = nlohmann::json;

// Allows you to access protected members from any class
// by generating a class that inherits the target class
// to then access the member.
// Takes in a pointer btw
// Example:
//   CCNode* foo;
//   public_cast(foo, m_fRotation) = 10.f;
#define public_cast(value, member) [](auto* v) { \
	class FriendClass__; \
	using T = std::remove_pointer<decltype(v)>::type; \
	class FriendeeClass__: public T { \
	protected: \
		friend FriendClass__; \
	}; \
	class FriendClass__ { \
	public: \
		auto& get(FriendeeClass__* v) { return v->member; } \
	} c; \
	return c.get(reinterpret_cast<FriendeeClass__*>(v)); \
}(value)

const char* get_frame_name(CCSprite* sprite_node) {
    auto* texture = sprite_node->getTexture();

    CCDictElement* el;

    auto* frame_cache = CCSpriteFrameCache::sharedSpriteFrameCache();
    auto* cached_frames = public_cast(frame_cache, m_pSpriteFrames);
    const auto rect = sprite_node->getTextureRect();
    CCDICT_FOREACH(cached_frames, el) {
            auto* frame = static_cast<CCSpriteFrame*>(el->getObject());
            if (frame->getTexture() == texture && frame->getRect() == rect) {
                return el->getStrKey();
            }
        };
    return "none";
}

void traverse_gameobject(CCNode* node, const CCSize& parent_content_size, json& json_object) {
    const auto children_count = node->getChildrenCount();
    if (auto sprite_node = dynamic_cast<CCSprite*>(node); sprite_node && std::string(get_frame_name(sprite_node)) != std::string("none")) {
        auto object = json::object();
        object["frame"] = get_frame_name(sprite_node);
        object["x"] = sprite_node->getPositionX() - parent_content_size.width / 2;
        object["y"] = sprite_node->getPositionY() - parent_content_size.height / 2;
        object["z"] = sprite_node->getZOrder();
        object["rot"] = -sprite_node->getRotation();
        object["anchor_x"] = sprite_node->getAnchorPoint().x - 0.5;
        object["anchor_y"] = sprite_node->getAnchorPoint().y - 0.5;
        object["scale_x"] = sprite_node->getScaleX();
        object["scale_y"] = sprite_node->getScaleY();
        object["flip_x"] = sprite_node->isFlipX();
        object["flip_y"] = sprite_node->isFlipY(); 
        if (sprite_node->getColor().r == 100) {
            object["color_channel"] = "base";
        } else if (sprite_node->getColor().b == 100) {
            object["color_channel"] = "detail";
        } else if (sprite_node->getColor().r == 0 && sprite_node->getColor().g == 0 && sprite_node->getColor().b == 0) {
            object["color_channel"] = "black";
        } else {
            auto rgbaNode = geode::cast::typeinfo_cast<CCRGBAProtocol*>(node);
            geode::log::debug("Unkown color: {}, {}, {}", rgbaNode->getColor().r, rgbaNode->getColor().g, rgbaNode->getColor().b);
        }
        auto children = node->getChildren();
        for (unsigned int i = 0; i < children_count; ++i) {
            auto child = children->objectAtIndex(i);
            traverse_gameobject(dynamic_cast<CCNode*>(child), sprite_node->getContentSize(), object["children"]);
        }
        json_object.push_back(object);
    }
}

void traverse(CCNode* node, json& json_object, std::unordered_set<int> visited) {
    const auto children_count = node->getChildrenCount();
    if (auto gob = dynamic_cast<GameObject *>(node); gob
            && !visited.contains(gob->m_objectID)
            && !(gob->m_objectID == 8 && gob->getPositionX() == 0 && gob->getPositionY() == 105) // avoid anticheat spike
        ) {
        visited.insert(gob->m_objectID);
        auto id_key = std::to_string(gob->m_objectID);
        json_object[id_key] = json::object();
        json_object[id_key]["frame"] = get_frame_name(gob);
        json_object[id_key]["object_type"] = gob->m_objectType;
        json_object[id_key]["default_z_layer"] = gob->m_defaultZLayer;
        json_object[id_key]["default_z_order"] = gob->m_defaultZOrder;
        auto newobject = json::object();
        newobject["minX"] = gob->m_objectRect.getMinX();
        newobject["midX"] = gob->m_objectRect.getMidX();
        newobject["maxX"] = gob->m_objectRect.getMaxX();
        newobject["minY"] = gob->m_objectRect.getMinY();
        newobject["midY"] = gob->m_objectRect.getMidY();
        newobject["maxY"] = gob->m_objectRect.getMaxY();
        newobject["origin"]["x"] = gob->m_objectRect.origin.x;
        newobject["origin"]["y"] = gob->m_objectRect.origin.y;
        newobject["size"]["width"] = gob->m_objectRect.size.width;
        newobject["size"]["height"] = gob->m_objectRect.size.height;
        json_object[id_key]["hitbox"] = newobject;
        if (gob->m_baseColor) {
            json_object[id_key]["default_base_color_channel"] = gob->m_baseColor->m_defaultColorID;
        } else {
            json_object[id_key]["default_base_color_channel"] = -1;
        }
        if (gob->m_detailColor) {
            json_object[id_key]["default_detail_color_channel"] = gob->m_detailColor->m_defaultColorID;
        } else {
            json_object[id_key]["default_detail_color_channel"] = -1;
        }
        if (gob->getColor().r == 100) {
            json_object[id_key]["color_channel"] = "base";
        } else if (gob->getColor().b == 100) {
            json_object[id_key]["color_channel"] = "detail";
        } else if (gob->getColor().r == 0 && gob->getColor().g == 0 && gob->getColor().b == 0) {
            json_object[id_key]["color_channel"] = "black";
        } else {
            auto rgbaNode = geode::cast::typeinfo_cast<CCRGBAProtocol*>(node);
            geode::log::debug("Unkown color: {}, {}, {} for ID {}", rgbaNode->getColor().r, rgbaNode->getColor().g, rgbaNode->getColor().b, gob->m_objectID);
        }
        auto children = node->getChildren();
        for (unsigned int i = 0; i < children_count; ++i) {
            auto child = children->objectAtIndex(i);
            traverse_gameobject(dynamic_cast<CCNode*>(child), gob->getContentSize(), json_object[id_key]["children"]);
        }
    } else {
        auto children = node->getChildren();
		for (unsigned int i = 0; i < children_count; ++i) {
			auto child = children->objectAtIndex(i);
			traverse(dynamic_cast<CCNode*>(child), json_object, visited);
		}
    }
}

class $modify(EUIHook, EditorUI) {
    void onButton(CCObject* sender) {
        geode::createQuickPopup("object.json", "For this to work correctly you need to have every single object visible on screen!", "Back", "Run", [](FLAlertLayer*, bool yes) {
            if (!yes) return;
            json json;
            std::unordered_set<int> visited = {914};
            traverse(CCScene::get()->getChildByIDRecursive("batch-layer"), json, visited);
            std::ofstream o(geode::Mod::get()->getSaveDir() / "object.json");
            o << json.dump(4);
            o.close();
            geode::createQuickPopup("object.json", "Dump finished successfully! Do you want to open the folder?", "No", "Yes", [](FLAlertLayer*, bool yes) {
                if(!yes) return;
                geode::utils::file::openFolder(geode::Mod::get()->getSaveDir());
            });
        });
    }
    
    bool init(LevelEditorLayer* editorLayer) {
        bool ret = EditorUI::init(editorLayer);
        auto menu = getChildByIDRecursive("settings-menu");
        if(!menu) return ret;

        auto spr = ButtonSprite::create("{}");
        auto editorButton = CCMenuItemSpriteExtra::create(spr, this, menu_selector(EUIHook::onButton));
        menu->addChild(editorButton);
        menu->updateLayout();
        return ret;
    }
};