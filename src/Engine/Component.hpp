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

    struct Model
    {
        ResourceManager::Entry<Engine::Model> model;
    };

    struct Camera
    {
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
    };

    struct Light
    {
        mn::Math::Vec3f color;
        float intensity;
    };
}