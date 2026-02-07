#include "my_net_config.h"
// ======================= 工具：发送 JSON 到某个客户端/全部 =======================
void wsSendTo(AsyncWebSocketClient *c, const JsonDocument &doc)
{
    String s;
    serializeJson(doc, s);
    if (c)
        c->text(s);
}

// ======================= 工具函数：WS 保活与广播 =======================
void my_wsheart()
{
    ws.cleanupClients(); // 清理断开的
    size_t n = ws.count();
    for (size_t i = 0; i < n; i++)
    {
        AsyncWebSocketClient *c = ws.client(i);
        if (!c)
            continue;
        if (c->canSend())
            c->ping(); // 发送 ping 帧
    }
    vTaskDelay(pdMS_TO_TICKS(15000)); // 每 15 秒 ping 一次
}

bool wsCanBroadcast()
{
    bool ok = true;
    size_t n = ws.count();
    for (size_t i = 0; i < n; i++)
    {
        AsyncWebSocketClient *c = ws.client(i);
        if (!c)
            continue;
        if (c->queueIsFull() || !c->canSend())
        {
            ok = false;
            break;
        }
    }
    return ok;
}

void wsBroadcast(const JsonDocument &doc)
{
    // 清理断开的客户端，避免幽灵连接占坑
    ws.cleanupClients();
    if (!wsCanBroadcast())
        return; // 丢掉这一帧，避免把队列越堆越高

    String s;
    s.reserve(320); // 预留缓冲，减少堆分配
    serializeJson(doc, s);
    ws.textAll(s);
}
