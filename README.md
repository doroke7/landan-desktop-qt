# UsbCam

以 Qt 6 (C++) 開發的桌面應用，用來接入 USB 視訊裝置並預覽、拍照、錄影。
預設格式為 **1280×720 @ 30fps**。

## 功能

- 自動列出 USB 視訊裝置，支援熱插拔
- 預設選用 1280×720、30fps 的格式；裝置不支援時自動退而選最接近的格式
- 可從下拉選單手動切換解析度 / 幀率 / 像素格式
- 開始 / 停止預覽
- 拍照（JPG）與錄影（MP4），存放在「影片」資料夾
- 狀態列顯示目前實際的解析度與幀率

## 需求

- Qt 6.5 以上（Widgets、Multimedia、MultimediaWidgets）
- CMake 3.21 以上
- C++17 編譯器

macOS 安裝：

```bash
brew install qt cmake
```

## 建置與執行

使用 Makefile：

```bash
make          # 設定並編譯
make run      # 編譯並開啟 UsbCam.app
make clean    # 刪除 build/
make rebuild  # 清除後重新編譯
```

可覆寫變數：`BUILD_DIR`、`BUILD_TYPE`（預設 Release）、`QT_PREFIX`（預設取 `brew --prefix qt`）。

或直接用 CMake：

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt)"
cmake --build build
open build/UsbCam.app
```

## 權限（macOS）

第一次開啟時系統會詢問攝影機權限，請按允許。若先前已拒絕，到
「系統設定 → 隱私權與安全性 → 攝影機」開啟，或執行：

```bash
tccutil reset Camera com.example.usbcam
```

實作注意事項：

- 建置後 CMake 會對 `UsbCam.app` 做 ad-hoc 簽署，讓 Info.plist 綁定到簽章；
  否則 macOS 不會正確處理攝影機權限，預覽會是黑畫面。
- 程式會先取得攝影機權限，才建立 `QCamera`；權限未確定時就建立，
  Qt 會回報「Access to camera not granted」，預覽同樣是黑的。

## 專案結構

```
CMakeLists.txt     建置設定
Makefile           CMake 的簡易包裝
Info.plist.in      macOS bundle 設定（含攝影機權限說明）
src/main.cpp       進入點
src/mainwindow.*   主視窗：裝置 / 格式選擇、預覽、拍照、錄影
```

## 已知限制

Qt 只能從裝置回報的格式清單中選擇，無法強制指定任意幀率。
部分 USB 攝影機在 720p 只提供特定像素格式（如 MJPEG）的 30fps，這是裝置本身的限制。

## 效能

1280×720 @ 30fps 預覽時，CPU 約 19–21%（MacBook Air 內建相機，實測）。

- 渲染已經走 GPU：`QVideoWidget` 在 macOS 使用 Qt RHI（Metal），NV12 轉色由 shader 完成，
  換成 `QQuickWidget` / QML `VideoOutput` 預期沒有明顯差異。
- 取樣顯示主要運算在 Apple 的 `Espresso`（Neural Engine），推測來自系統相機的視訊效果
  （人像、居中顯示、攝影棚燈光），而非本程式的程式碼；此為推測，尚未用 Instruments 證實。
- 想降低 CPU：關閉控制中心的「視訊效果」，或在格式選單改選較低解析度（如 640×480）。
