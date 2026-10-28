#include "Geode/loader/GameEvent.hpp"
#include <Geode/Geode.hpp>
#include <algorithm>
#include <cmath>
#include <Geode/cocos/touch_dispatcher/CCTouchDispatcher.h>
#include <Geode/modify/FMODAudioEngine.hpp>
#include <Geode/modify/LevelEditorLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/CCKeyboardDispatcher.hpp>
#include <Geode/modify/CCScene.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/CustomSongWidget.hpp>
#include <Geode/modify/EditorUI.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/modify/CCDirector.hpp>

#include "managers/MusicOverlayManager.cpp"
#include "managers/PlaybackManager.hpp"
#include "ui/linkSpotify.hpp"
#include "managers/httpManager.hpp"

#ifdef GEODE_IS_WINDOWS

#include <Windows.h>

#endif

#define WINRT_CPPWINRT

bool m_rebindWindowOpen = false;
bool g_musicOverlayVisible = false;
async::TaskHolder<web::WebResponse> m_listener;

$on_mod(Loaded) {
    #ifdef GEODE_IS_WINDOWS
    if(PlaybackManager::get().isWindows()) PlaybackManager::get().getMediaManager();

    GameEvent(geode::GameEventType::Exiting).listen([] {
        log::debug("Goodbye!");
        PlaybackManager::get().removeMediaManager();
    }).leak();
    #endif

    BitmapFont::load("font_default.fnt"_spr);
    BitmapFont::load("font_cyrillic.fnt"_spr);
    BitmapFont::load("font_greek.fnt"_spr);
    BitmapFont::load("font_japanese.fnt"_spr);
    BitmapFont::load("font_thai.fnt"_spr);
    BitmapFont::load("font_vietnamese.fnt"_spr);

    if (Mod::get()->getSavedValue<bool>("hasAuthorizedAndroid") && !PlaybackManager::get().isWindows()) {
        SpotifyAuth::get()->refresh([](std::string newToken) {
            log::debug("Token refreshed on load");
        });
    }

    listenForKeybindSettingPresses("key_overlay", [](Keybind key, bool down, bool repeat, double time) {
        if (!key.key) return;
        if (!CCScene::get()) return;

        auto overlay = MusicControlOverlay::get();
        if (!overlay) return;
        if (down && !repeat) {
            g_musicOverlayVisible = true;
            overlay->updateValues(true);
        } else if (!down) {
            g_musicOverlayVisible = false;
            overlay->updateValues(false);
        }
    });

    GameEvent(geode::GameEventType::Loaded).listen([] {
        if (Mod::get()->getSavedValue<bool>("hasAuthorizedAndroid") == false && PlaybackManager::get().isWindows() == false) {
            createQuickPopup("Link Spotify", "Please link your Spotify account to use Music Integrations <cc>(You will have 2 minutes to authenticate)</c>", "Close", "Link Spotify",
                [](auto, bool btn2) {
                    log::info("Spotify authorization popup response: {}", btn2 ? "Link Spotify" : "Close");
                    if (!btn2) return;
                    log::info("Starting Spotify authorization server");
                    SpotifyAuth::get()->start([](std::string code) {
                        auto req = web::WebRequest();

                        m_listener.spawn(
                            req.post("https://craftify.thatgravyboat.tech/api/v1/public/auth?type=auth&code=" + code, geode::getMod()),
                            [](web::WebResponse value) {
                                auto jsonResult = value.json();
                                if (!jsonResult.isOk()) {
                                    log::error("Failed to parse JSON response");
                                    return;
                                }
                                
                                auto jsonUnwrap = jsonResult.unwrap();
                                if (!jsonUnwrap.contains("access_token")) {
                                    log::error("No access_token in response!");
                                    return;
                                }
                                
                                auto tokenResult = jsonUnwrap["access_token"].asString();
                                if (!tokenResult.isOk()) {
                                    log::error("Failed to get access_token as string");
                                    return;
                                }

                                auto refreshResult = jsonUnwrap["refresh_token"].asString();
                                if (!refreshResult.isOk()) {
                                    log::error("Failed to get refresh_token as string");
                                    return;
                                }
                                
                                std::string token = tokenResult.unwrap();
                                std::string refresh = refreshResult.unwrap();
                                Mod::get()->setSavedValue("spotify-token", token);
                                Mod::get()->setSavedValue("spotify-refresh", refresh);
                                log::info("Successfully saved Spotify token");
                                Mod::get()->setSavedValue("hasAuthorizedAndroid", true);
                            }
                        );
                    });
                    log::info("Opening Spotify authorization page");
                    web::openLinkInBrowser("https://accounts.spotify.com/authorize?client_id=7314a0ab3c734b2caa4b483032cdd91f&response_type=code&redirect_uri=http%3A%2F%2F127.0.0.1%3A21851%2F&scope=user-read-playback-state%20user-modify-playback-state");
                }
            );
        }

        PlaybackManager::RateLimitUpdate("rate-limit-update"_spr).listen([] {
            log::debug("Rate limit hit!");
            CCSprite* warning = CCSprite::create("/foxy_1.png"_spr);
            CCSize winSize = CCDirector::get()->getWinSize();
            float scaleRatio = (winSize.height / warning->getContentSize().height);
            warning->setScaleX(scaleRatio);
            warning->setScaleY(scaleRatio);
            CCScene::get()->addChildAtPosition(warning, Anchor::Center, {0, 0}, false);
            CCArray* frames = CCArray::create();
            for (int i = 1; i <= 14; i++) {
                auto filename = fmt::format("/foxy_{}.png"_spr, i);
                auto texture = CCTextureCache::sharedTextureCache()->addImage(filename.c_str(), false);
                if (texture) {
                    auto frame = CCSpriteFrame::createWithTexture(texture, CCRectMake(0, 0, texture->getContentSize().width, texture->getContentSize().height));
                    frames->addObject(frame);
                }
            }

            CCAnimation* anim = CCAnimation::createWithSpriteFrames(frames, 0.1f);
            CCAnimate* animate = CCAnimate::create(anim);

            warning->runAction(
                CCSequence::create(
                    CallFuncExt::create([]() {
                        FMODAudioEngine::sharedEngine()->playEffect("/fnafjumpscare.ogg"_spr);
                    }),
                    animate,
                    CallFuncExt::create([warning]() {
                        warning->removeFromParent();
                    }),
                    nullptr
                )
            );
        }).leak();

    }).leak(); 
}


//End goal is to stop relying on this
class $modify(FMODAudioEngine) {
    void update(float p0) {
        FMODAudioEngine::update(p0);
        if (!Mod::get()->getSavedValue<bool>("autoEnabled") || PlayLayer::get()) return;
        auto lel = LevelEditorLayer::get();
        if (lel && lel->m_playbackActive) return;
        static bool lastMusicState = false;

        bool currentMusicState = FMODAudioEngine::isMusicPlaying(0);
        if (currentMusicState != lastMusicState) {
            PlaybackManager& pbm = PlaybackManager::get();
            if (currentMusicState) {
                pbm.control(false);
            } else {
                pbm.control(true);
            }
            lastMusicState = currentMusicState;
        }

    }
};

class DraggableMusicToggle : public CCLayer {
protected:
    CCSprite* m_background = nullptr;
    CCSprite* m_icon = nullptr;
    CCLabelBMFont* m_label = nullptr;
    CCPoint m_touchStart = CCPointZero;
    CCPoint m_startPosition = CCPointZero;
    CCPoint m_targetPosition = CCPointZero;
    bool m_dragged = false;
    bool m_touchingButton = false;

public:
    static DraggableMusicToggle* create() {
        auto ret = new DraggableMusicToggle();
        if (ret && ret->init()) {
            ret->autorelease();
            return ret;
        }
        CC_SAFE_DELETE(ret);
        return nullptr;
    }

    bool init() override {
        if (!CCLayer::init()) return false;

        auto size = CCDirector::sharedDirector()->getWinSize();
        this->setContentSize(size);
        this->setTouchEnabled(true);

        // Use the mod's own menu artwork. Geode selects the matching
        // -hd/-uhd resource variant for the device resolution.
        m_background = CCSprite::create("MH_menu.png"_spr);
        if (!m_background) {
            log::error("Music Integrations: failed to load MH_menu.png resource");
            return false;
        }

        m_background->setPosition({54.f, size.height * 0.62f});
        m_background->setOpacity(125); // 49% opacity while idle.
        this->addChild(m_background, 0);
        m_targetPosition = m_background->getPosition();
        this->scheduleUpdate();
        return true;
    }

    void registerWithTouchDispatcher() override {
        CCTouchDispatcher::get()->addTargetedDelegate(this, kCCMenuHandlerPriority - 1, true);
    }

    bool ccTouchBegan(CCTouch* touch, CCEvent* event) override {
        if (!m_background || !touch) return false;
        auto point = this->convertToNodeSpace(touch->getLocation());
        auto bounds = m_background->boundingBox();
        bounds.origin.x -= 14.f;
        bounds.origin.y -= 14.f;
        bounds.size.width += 28.f;
        bounds.size.height += 28.f;
        if (!bounds.containsPoint(point)) return false;

        m_touchingButton = true;
        m_background->setOpacity(255); // Restore normal opacity while pressed/held.
        m_dragged = false;
        m_touchStart = point;
        m_startPosition = m_background->getPosition();
        m_targetPosition = m_startPosition;
        return true;
    }

    void ccTouchMoved(CCTouch* touch, CCEvent* event) override {
        if (!m_touchingButton || !m_background || !touch) return;
        auto point = this->convertToNodeSpace(touch->getLocation());
        float dx = point.x - m_touchStart.x;
        float dy = point.y - m_touchStart.y;
        if (std::abs(dx) > 8.f || std::abs(dy) > 8.f) m_dragged = true;
        if (!m_dragged) return;

        auto size = this->getContentSize();
        auto bounds = m_background->boundingBox();
        float halfWidth = bounds.size.width * 0.5f;
        float halfHeight = bounds.size.height * 0.5f;
        float x = std::clamp(m_startPosition.x + dx, halfWidth, size.width - halfWidth);
        float y = std::clamp(m_startPosition.y + dy, halfHeight, size.height - halfHeight);
        // Keep the finger's destination as a target; update() eases the button
        // toward it over several frames for a smoother, slightly trailing feel.
        m_targetPosition = CCPoint(x, y);
    }

    void update(float dt) override {
        CCLayer::update(dt);
        if (!m_background || !m_dragged) return;
        auto current = m_background->getPosition();
        // Frame-rate independent smoothing; roughly 100 ms of gentle follow lag.
        float blend = 1.f - std::exp(-12.f * std::min(dt, 0.1f));
        m_background->setPosition({
            current.x + (m_targetPosition.x - current.x) * blend,
            current.y + (m_targetPosition.y - current.y) * blend
        });
    }

    void ccTouchEnded(CCTouch* touch, CCEvent* event) override {
        if (!m_touchingButton) return;
        m_touchingButton = false;
        if (m_background) m_background->setOpacity(125); // 49% opacity when idle.
        if (!m_dragged) {
            g_musicOverlayVisible = !g_musicOverlayVisible;
            auto overlay = MusicControlOverlay::get();
            if (overlay) overlay->updateValues(g_musicOverlayVisible);
        }
    }

    void ccTouchCancelled(CCTouch* touch, CCEvent* event) override {
        m_touchingButton = false;
        if (m_background) m_background->setOpacity(125);
    }
};

class $modify(LevelEditorLayer) {
    void onPlaytest() {        
        LevelEditorLayer::onPlaytest();
        if (!Mod::get()->getSavedValue<bool>("autoEnabled")) return;
        auto& pbm = PlaybackManager::get();
        pbm.control(false);
        pbm.m_immune = true;
    }
    void onStopPlaytest() {
        auto playtest = GJBaseGameLayer::get()->m_playbackMode;
        LevelEditorLayer::onStopPlaytest();
        if (!Mod::get()->getSavedValue<bool>("autoEnabled") || static_cast<int>(playtest) == 0) return;
        auto& pbm = PlaybackManager::get();
        pbm.m_immune = false;
        if(!FMODAudioEngine::sharedEngine()->isMusicPlaying(0)) pbm.control(true);
    }
    bool init(GJGameLevel* p0, bool p1) {
        if (!LevelEditorLayer::init(p0, p1)) return false;
        if (!Mod::get()->getSavedValue<bool>("autoEnabled")) return true;
        auto& pbm = PlaybackManager::get();
        pbm.m_immune = false;
        pbm.control(true);
        return true;
    }
};

class $modify(PlayLayer) {
    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;
        if (!Mod::get()->getSavedValue<bool>("autoEnabled")) return true;
        auto& pbm = PlaybackManager::get();
        pbm.control(false);
        pbm.m_immune = true;
        return true;
    }
    void onQuit() {
        PlayLayer::onQuit();
        if (!Mod::get()->getSavedValue<bool>("autoEnabled")) return;
        auto& pbm = PlaybackManager::get();
        pbm.m_immune = false;
        if(!FMODAudioEngine::sharedEngine()->isMusicPlaying(0)) pbm.control(true);
    }
};

class $modify (EditorUI) {
    void onPlayback(CCObject* sender) {
        EditorUI::onPlayback(sender);
        if (!Mod::get()->getSavedValue<bool>("autoEnabled")) return;
        PlaybackManager& pbm = PlaybackManager::get();
        if (EditorUI::m_playbackActive) {
            pbm.control(false);
        } else {
            pbm.control(true);
        }
    }
};

// Add the music toggle at scene level so it is available in the main menu,
// Geode's menu, and level-related menus instead of only MenuLayer.
class $modify(CCScene) {
    void onEnter() {
        CCScene::onEnter();

        // Do not show the floating menu button while playing or editing a level.
        if (PlayLayer::get() || LevelEditorLayer::get()) return;

        // Scene transitions/re-entry must not create duplicate buttons.
        if (this->getChildByID("music-integrations-menu-toggle")) return;

        auto toggle = DraggableMusicToggle::create();
        if (toggle) {
            toggle->setID("music-integrations-menu-toggle");
            this->addChild(toggle, 999);
            log::info("Music Integrations: menu button added to scene");
        } else {
            log::error("Music Integrations: failed to create scene menu button");
        }
    }
};
