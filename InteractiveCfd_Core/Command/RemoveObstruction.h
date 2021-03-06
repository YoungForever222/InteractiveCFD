#pragma once
#include "Command.h"

class FW_API RemoveObstruction : public Command
{
    int m_currentObst;
public:
    RemoveObstruction(Panel &rootPanel);
    void Start(const float currentX, const float currentY);
    void End(const float currentX, const float currentY);
};