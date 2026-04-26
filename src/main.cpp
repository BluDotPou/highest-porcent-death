#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/LevelInfoLayer.hpp>

#include <algorithm>
#include <map>
#include <string>
#include <vector>

using namespace geode::prelude;

static constexpr int kTopDeathLabelTag = 0x7A71;

static std::string getLevelKey(GJGameLevel* level) {
    return std::to_string(level->m_levelID);
}

static std::map<int, int> loadDeathsForLevel(GJGameLevel* level) {
    std::map<int, int> deaths;
    if (!level) return deaths;

    auto saved = Mod::get()->getSavedValue<matjson::Value>(
        getLevelKey(level),
        matjson::Value::object()
    );

    for (auto const& [k, v] : saved) {
        int percent = 0;
        try {
            percent = std::stoi(k);
        } catch (...) {
            continue;
        }

        deaths[percent] = v.asInt().unwrapOr(0);
    }

    return deaths;
}

static void saveDeathsForLevel(GJGameLevel* level, std::map<int, int> const& deaths) {
    if (!level) return;

    matjson::Value obj = matjson::Value::object();
    for (auto const& [percent, count] : deaths) {
        obj[std::to_string(percent)] = count;
    }

    Mod::get()->setSavedValue(getLevelKey(level), obj);
}

static CCPoint getLabelPosition(LevelInfoLayer* self) {
    //keep the label close to the real stats block
    if (self->m_orbsLabel) {
        auto p = self->m_orbsLabel->getPosition();
        return {p.x, p.y - 22.f};
    }

    if (self->m_likesLabel) {
        auto p = self->m_likesLabel->getPosition();
        return {p.x, p.y - 22.f};
    }

    if (self->m_lengthLabel) {
        auto p = self->m_lengthLabel->getPosition();
        return {p.x, p.y - 22.f};
    }

    //fallback if the ui is being dramatic
    return {20.f, 120.f};
}

static std::string buildTopDeathsText(GJGameLevel* level) {
    auto deaths = loadDeathsForLevel(level);
    int amount = Mod::get()->getSettingValue<int>("top-death-count");

    std::vector<std::pair<int, int>> sorted(deaths.begin(), deaths.end());
    std::sort(sorted.begin(), sorted.end(), [](auto const& a, auto const& b) {
        return a.second > b.second;
    });

    std::string text = "top deaths:\n";

    if (sorted.empty()) {
        text += "no deaths yet";
        return text;
    }

    for (int i = 0; i < std::min(amount, static_cast<int>(sorted.size())); ++i) {
        text += fmt::format("{}% - {}\n", sorted[i].first, sorted[i].second);
    }

    return text;
}

static void updateTopDeathsLabel(LevelInfoLayer* self) {
    auto* level = self->m_level;
    if (!level) return;

    auto* label = static_cast<CCLabelBMFont*>(self->getChildByTag(kTopDeathLabelTag));
    if (!label) {
        label = CCLabelBMFont::create("", "bigFont.fnt");
        label->setTag(kTopDeathLabelTag);
        label->setAnchorPoint({0.f, 0.5f});
        label->setScale(0.33f);
        self->addChild(label, 10);
    }

    label->setString(buildTopDeathsText(level).c_str());
    label->setPosition(getLabelPosition(self));
}

class $modify(HighestDeathPlayLayer, PlayLayer) {
    struct Fields {
        bool m_isPractice = false;
    };
    
    

    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        m_fields->m_isPractice = useReplay;
        return PlayLayer::init(level, useReplay, dontCreateObjects);
    }

    void destroyPlayer(PlayerObject* player, GameObject* object) {
        auto* level = this->m_level;
        if (!level) {
            PlayLayer::destroyPlayer(player, object);
            return;
        }

        bool countPractice = Mod::get()->getSettingValue<bool>("Enable-practice");

        if (!m_fields->m_isPractice || countPractice) {
            int percent = this->getCurrentPercentInt();
            if (percent > 0) {
                auto deaths = loadDeathsForLevel(level);
                deaths[percent]++;
                saveDeathsForLevel(level, deaths);
            }
        }

        PlayLayer::destroyPlayer(player, object);
    }
};

class $modify(HighestDeathLevelInfoLayer, LevelInfoLayer) {
    bool init(GJGameLevel* level, bool challenge) {
        return LevelInfoLayer::init(level, challenge);
    }

    void setupLevelInfo() {
        LevelInfoLayer::setupLevelInfo();
        updateTopDeathsLabel(this);
    }

    void updateLabelValues() {
        LevelInfoLayer::updateLabelValues();
        updateTopDeathsLabel(this);
    }
};