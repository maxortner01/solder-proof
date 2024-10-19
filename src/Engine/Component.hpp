#pragma once

#include <midnight/midnight.hpp>

#include "ResourceManager.hpp"
#include "Model.hpp"

/*
namespace Engine::Rendering
{
    struct GBuffer
    {
        std::shared_ptr<mn::Graphics::Image> position, normals, albedo, specular, lighting;
    };
}*/

namespace Engine::Component
{
    struct Transform
    {
        mn::Math::Vec3f position, scale;
        mn::Math::Vec3<mn::Math::Angle> rotation;
    };

    struct Hidden { };
    struct DontCull { };

    struct Model
    {
        bool lit;
        ResourceManager::Entry<Engine::Model> model;
    };

    struct Camera
    {
        enum Type
        {
            FPS, Orbit
        } type;

        float orbitDistance;
        mn::Math::Angle FOV;
        mn::Math::Vec2f near_far;
        std::shared_ptr<mn::Graphics::Image> surface;

        static Camera make(mn::Math::Vec2u size, mn::Math::Angle FOV, mn::Math::Vec2f nf)
        {
            Camera c;
            c.FOV = FOV;
            c.near_far = nf;
            c.surface = std::make_shared<mn::Graphics::Image>(
                mn::Graphics::ImageFactory()
                    .addAttachment<mn::Graphics::Image::Color>(mn::Graphics::Image::B8G8R8A8_UNORM, size)
                    .addAttachment<mn::Graphics::Image::DepthStencil>(mn::Graphics::Image::DF32_SU8, size)
                    .build()
            );
            return c;
        }

        mn::Math::Mat4<float> 
        createViewMatrix(const Transform& transform) const
        {
            switch (type)
            {
            case Orbit:
                return mn::Math::translation(transform.position * -1.f) *
                       mn::Math::rotation<float>(transform.rotation * -1.f) *
                       mn::Math::translation(mn::Math::Vec3f{ 0.f, 0.f, -orbitDistance });
            case FPS:
                return mn::Math::translation(transform.position * -1.f) * 
                       mn::Math::rotation<float>(transform.rotation * -1.0);
            }
        }
    };

    struct Light
    {
        mn::Math::Vec3f color;
        float intensity;
    };
}