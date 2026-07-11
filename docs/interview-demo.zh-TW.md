# 面試展示講稿（10–15 分鐘）

## 1. 問題與安全邊界

瀏覽器退到背景後仍可能保留大量常駐頁面；遊戲、編譯、EDA 或本機模型推論需要 RAM 時，`browser_guard` 會降低背景瀏覽器的資源壓力。

但原生 Windows 程式看不到分頁內部語意，所以我沒有把目標定成「凍得最兇」，而是：

> 降低背景資源壓力，同時避免管理程式失敗後把瀏覽器永久留在凍結狀態。

## 2. 三行程架構

- `browser_guard.exe`：掃描瀏覽器、執行策略、控制行程。
- `browser_guard_control.exe`：透過有 ACL 的 Named Event 要求正常關閉，不使用強制終止。
- `browser_guard_recovery.exe`：獨立監控 Guard；Guard crash 時驗證 journal 並恢復行程。

Suspend 前會先原子寫入 PID、建立時間、Session、完整路徑；Resume 成功後才刪除。建立時間可防止 PID 被重用後恢復到錯誤行程。

## 3. C／Windows API 技術點

- Tool Help Snapshot 與 Access Token：行程發現、使用者與 Session 限制。
- Core Audio：保護有輸出音訊的瀏覽器。
- PSAPI、Memory Priority、EcoQoS：記憶體與背景資源政策。
- `NtSuspendProcess`／`NtResumeProcess`：動態載入並明確標記為實驗性 API。
- Named Event＋SDDL：Controller IPC 僅允許目前使用者。
- 原子 journal：暫存檔 flush 後 replace，避免 crash 時留下半份資料。

## 4. 測試展示

```powershell
cmake -S . -B build -A x64 -DBG_WARNINGS_AS_ERRORS=ON
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

重點說明：

- 一個 Chrome 視窗最小化、另一個仍顯示時，整個 Chrome 家族不得 suspend。
- Crash Recovery 測試用專屬 fixture，不碰真實瀏覽器；fixture 一開始就是 suspended，擁有者突然退出後，只有 Broker 正確恢復才會產生成功檔案。

## 5. Benchmark 誠實性

舊版只有單次 Baseline→Guarded，且宣傳已經很低的 Working Set 再下降 96%。新版改成 AB/BA 交錯、多次 trial，報告平均、標準差、CPU、Page Fault 與恢復延遲。

Working Set 下降只代表實體 RAM 駐留頁面減少，不代表 Private Bytes 被釋放；重新載入頁面也會付出 Page Fault 與卡頓成本。

## 6. 主動說明限制

OS 行程層無法可靠辨識下載、靜音影片、會議、麥克風、未儲存表單或長時間網頁運算。因此預設只處理「所有可見視窗都最小化」的瀏覽器；Aggressive 模式必須明確開啟。

下一步是移除週期性 Resume heartbeat、記錄完整背景政策供 Broker 還原，長期再用 Browser Extension＋Native Messaging 取得分頁語意。
