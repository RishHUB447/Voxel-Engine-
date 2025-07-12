#ifndef ANIMATOR_H
#define ANIMATOR_H

#include "Animation.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <vector>
#include <string>
#include <algorithm>
#include <cassert>

class Animator {
public:
    struct AnimationState {
        Animation* anim = nullptr;
        float currentTime = 0.0f;
        float weight = 1.0f;
        float speed = 1.0f;
        bool loop = true;
        bool enabled = true;
    };

    Animator() = default;

    /**
     * Play an animation immediately (no blend) or cross-fade if blendTime > 0
     * If this is the first animation added, initializes bone count from its bone map
     */
    void PlayAnimation(Animation* animation, float blendTime = 0.0f) {
        initializeBoneCount(animation);
        if (blendTime <= 0.0f) {
            m_ActiveAnimations.clear();
            m_ActiveAnimations.push_back({ animation, 0.0f, 1.0f, 1.0f, true, true });
        }
        else {
            BlendAnimation(animation, 1.0f, 1.0f, true);
        }
    }

    /**
     * Layer or cross-fade an animation with specified weight
     * Automatically initializes bone count on first use
     */
    void BlendAnimation(Animation* animation, float weight,
        float speed = 1.0f, bool loop = true) {
        initializeBoneCount(animation);
        for (auto& state : m_ActiveAnimations) {
            if (state.anim == animation) {
                state.weight = weight;
                state.speed = speed;
                state.loop = loop;
                state.enabled = true;
                NormalizeWeights();
                return;
            }
        }
        m_ActiveAnimations.push_back({ animation, 0.0f, weight, speed, loop, true });
        NormalizeWeights();
    }

    /**
     * Stop and remove a specified animation layer immediately
     */
    void StopAnimation(Animation* animation) {
        m_ActiveAnimations.erase(
            std::remove_if(m_ActiveAnimations.begin(), m_ActiveAnimations.end(),
                [animation](auto& s) { return s.anim == animation; }),
            m_ActiveAnimations.end());
        NormalizeWeights();
    }

    /**
     * Advance all animation states, remove disabled ones, and recompute bone matrices
     * Call once per frame
     */
    void UpdateAnimation(float dt) {
        for (auto& s : m_ActiveAnimations) {
            if (!s.enabled) continue;
            s.currentTime += s.anim->GetTicksPerSecond() * dt * s.speed;
            if (s.loop) {
                s.currentTime = fmod(s.currentTime, s.anim->GetDuration());
            }
            else if (s.currentTime >= s.anim->GetDuration()) {
                s.currentTime = s.anim->GetDuration();
                s.enabled = false;
            }
        }
        m_ActiveAnimations.erase(
            std::remove_if(m_ActiveAnimations.begin(), m_ActiveAnimations.end(),
                [](auto& s) { return !s.enabled; }),
            m_ActiveAnimations.end());

        m_FinalBoneMatrices.assign(m_BoneCount, glm::mat4(1.0f));
        if (m_ActiveAnimations.empty()) return;

        CalculateBlendedNodeTransforms(&m_ActiveAnimations.front().anim->GetRootNode(), glm::mat4(1.0f));
    }

    /**
     * Get the final bone transform matrices for skinning
     */
    const std::vector<glm::mat4>& GetFinalBoneMatrices() const {
        return m_FinalBoneMatrices;
    }

private:
    size_t m_BoneCount = 0;
    std::vector<AnimationState> m_ActiveAnimations;
    std::vector<glm::mat4> m_FinalBoneMatrices;

    // If bone count not set, derive from animation's bone map
    void initializeBoneCount(Animation* animation) {
        if (m_BoneCount == 0 && animation) {
            m_BoneCount = animation->GetBoneIDMap().size();
            assert(m_BoneCount > 0);
            m_FinalBoneMatrices.assign(m_BoneCount, glm::mat4(1.0f));
        }
    }

    void NormalizeWeights() {
        float sum = 0.0f;
        for (auto& s : m_ActiveAnimations) if (s.enabled) sum += s.weight;
        if (sum > 0.0f) for (auto& s : m_ActiveAnimations) if (s.enabled) s.weight /= sum;
    }

    // Recursively blend and write bone transforms
    void CalculateBlendedNodeTransforms(const AssimpNodeData* node, const glm::mat4& parentTransform) {
        const std::string& name = node->name;
        std::vector<glm::vec3> scales;
        std::vector<glm::vec3> trans;
        std::vector<glm::quat> rots;
        std::vector<float> weights;

        for (auto& s : m_ActiveAnimations) {
            if (!s.enabled || s.weight <= 0.0f) continue;
            glm::mat4 local = node->transformation;
            if (Bone* b = s.anim->FindBone(name)) {
                b->Update(s.currentTime);
                local = b->GetLocalTransform();
            }
            glm::vec3 scale, translation, skew;
            glm::quat rotation;
            glm::vec4 perspective;
            glm::decompose(local, scale, rotation, translation, skew, perspective);

            scales.push_back(scale);
            trans.push_back(translation);
            if (!rots.empty() && glm::dot(rots[0], rotation) < 0.0f) rotation = -rotation;
            rots.push_back(rotation);
            weights.push_back(s.weight);
        }

        glm::vec3 blendedScale(0.0f);
        glm::vec3 blendedTrans(0.0f);
        for (size_t i = 0; i < weights.size(); ++i) {
            blendedScale += scales[i] * weights[i];
            blendedTrans += trans[i] * weights[i];
        }
        glm::quat blendedRot(1, 0, 0, 0);
        if (!rots.empty()) {
            blendedRot = rots[0];
            float accum = weights[0];
            for (size_t i = 1; i < rots.size(); ++i) {
                float w = weights[i];
                float factor = w / (accum + w);
                blendedRot = glm::slerp(blendedRot, rots[i], factor);
                accum += w;
            }
        }

        glm::mat4 localBlend = glm::translate(glm::mat4(1.0f), blendedTrans)
            * glm::toMat4(blendedRot)
            * glm::scale(glm::mat4(1.0f), blendedScale);
        glm::mat4 globalTransform = parentTransform * localBlend;

        auto& boneMap = m_ActiveAnimations.front().anim->GetBoneIDMap();
        auto it = boneMap.find(name);
        if (it != boneMap.end()) {
            int idx = it->second.id;
            assert(idx >= 0 && idx < static_cast<int>(m_BoneCount));
            if (idx >= 0 && idx < static_cast<int>(m_BoneCount)) {
                m_FinalBoneMatrices[idx] = globalTransform * it->second.offset;
            }
        }

        for (int i = 0; i < node->childrenCount; ++i) {
            CalculateBlendedNodeTransforms(&node->children[i], globalTransform);
        }
    }
};

#endif // ANIMATOR_H
