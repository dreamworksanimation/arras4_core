// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "Component.h"
#include <client/api/Client.h>

using namespace arras4::client;

Component::Component(Client& aClient)
    : mClient(aClient)
{
    mClient.addComponent(this);
}

Component::~Component()
{
    mClient.removeComponent(this);
}

void
Component::onMessage(const api::Message& /*aMsg*/)
{
}

void
Component::onStatusMessage(const api::Message& /*aMsg*/)
{
}

void
Component::onEngineReady()
{
}

void
Component::send(const api::MessageContentConstPtr& aMsg)
{
    mClient.send(aMsg);
}

