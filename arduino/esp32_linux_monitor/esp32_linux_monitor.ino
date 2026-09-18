#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_SDA 21
#define OLED_SCL 22
#define OLED_ADDR 0x3C
#define LED_PIN 2

// ============================================================
// ALERTAS V5 - SIN CAMBIOS
// ============================================================

#define WARN_CPU 80
#define WARN_RAM 85
#define WARN_FAN 4500
#define WARN_NET_MB 10

#define CRIT_CPU 95
#define CRIT_RAM 90
#define CRIT_FAN 5200
#define CRIT_NET_MB 30

#define WARN_BLINK_MS 500
#define CRIT_BLINK_MS 120

Adafruit_SSD1306 display(
  SCREEN_WIDTH,
  SCREEN_HEIGHT,
  &Wire,
  -1
);

// ============================================================
// METRICAS V5
// ============================================================

int cpu = 0;
int ram = 0;
int cpuTemp = 0;
int fan = 0;

int gpu = 0;
int gpuTemp = 0;

int diskPercent = 0;
int nvmeTemp = 0;

int battery = 0;
int plugged = 0;

unsigned long hostUptime = 0;

unsigned long downloadBps = 0;
unsigned long uploadBps = 0;

// ============================================================
// DEV V6
// ============================================================

int wakaMinutes = 0;

String devProject = "--";
String devBranch = "--";

int gitModified = 0;

// ============================================================
// TIMERS
// ============================================================

unsigned long lastData = 0;
unsigned long lastRefresh = 0;
unsigned long lastPageChange = 0;

const unsigned long PAGE_TIME = 5000;

// V6:
// 0 SYSTEM
// 1 GPU
// 2 STORAGE
// 3 NETWORK
// 4 DEV

int currentPage = 0;

// ============================================================
// ALERTAS
// ============================================================

enum AlertLevel {
  ALERT_NORMAL,
  ALERT_WARNING,
  ALERT_CRITICAL
};

AlertLevel alertLevel = ALERT_NORMAL;

unsigned long lastLedToggle = 0;

bool ledState = false;

// ============================================================
// HISTORICOS
// ============================================================

#define HISTORY_SIZE 64

uint8_t gpuHistory[HISTORY_SIZE];

uint16_t netHistory[HISTORY_SIZE];

int gpuHistoryIndex = 0;
int netHistoryIndex = 0;

bool gpuHistoryFilled = false;
bool netHistoryFilled = false;

// ============================================================
// ALERT LEVEL
// ============================================================

void updateAlertLevel() {

  unsigned long totalNetwork =
    downloadBps + uploadBps;

  if (
    cpu >= CRIT_CPU ||
    ram >= CRIT_RAM ||
    fan >= CRIT_FAN ||
    totalNetwork >=
      CRIT_NET_MB * 1024UL * 1024UL
  ) {

    alertLevel = ALERT_CRITICAL;

    return;
  }

  if (
    cpu >= WARN_CPU ||
    ram >= WARN_RAM ||
    fan >= WARN_FAN ||
    totalNetwork >=
      WARN_NET_MB * 1024UL * 1024UL
  ) {

    alertLevel = ALERT_WARNING;

    return;
  }

  alertLevel = ALERT_NORMAL;
}

// ============================================================
// LED
// ============================================================

void updateAlertLed() {

  if (
    millis() - lastData > 3000 ||
    alertLevel == ALERT_NORMAL
  ) {

    ledState = false;

    digitalWrite(
      LED_PIN,
      LOW
    );

    return;
  }

  unsigned long interval =
    (
      alertLevel == ALERT_CRITICAL
    )
    ? CRIT_BLINK_MS
    : WARN_BLINK_MS;

  if (
    millis() - lastLedToggle >= interval
  ) {

    lastLedToggle = millis();

    ledState = !ledState;

    digitalWrite(
      LED_PIN,
      ledState ? HIGH : LOW
    );
  }
}

// ============================================================
// BAR
// ============================================================

void drawBar(
  int x,
  int y,
  int width,
  int height,
  int value
) {

  value = constrain(
    value,
    0,
    100
  );

  display.drawRect(
    x,
    y,
    width,
    height,
    SSD1306_WHITE
  );

  int filled = map(
    value,
    0,
    100,
    0,
    width - 4
  );

  if (filled > 0) {

    display.fillRect(
      x + 2,
      y + 2,
      filled,
      height - 4,
      SSD1306_WHITE
    );
  }
}

// ============================================================
// HEADER
// ============================================================

void drawHeader(
  const char* title
) {

  display.setTextSize(1);

  display.setTextColor(
    SSD1306_WHITE
  );

  display.setCursor(
    0,
    0
  );

  display.print(title);

  if (
    alertLevel == ALERT_WARNING
  ) {

    display.setCursor(
      70,
      0
    );

    display.print("!");

  } else if (
    alertLevel == ALERT_CRITICAL
  ) {

    display.setCursor(
      67,
      0
    );

    display.print("!!");
  }

  if (
    millis() - lastData < 3000
  ) {

    display.setCursor(
      88,
      0
    );

    display.print("USB");

    display.fillCircle(
      123,
      3,
      2,
      SSD1306_WHITE
    );

  } else {

    display.setCursor(
      80,
      0
    );

    display.print("OFFLINE");
  }

  display.drawLine(
    0,
    10,
    127,
    10,
    SSD1306_WHITE
  );
}

// ============================================================
// SYSTEM
// ============================================================

void drawSystemPage() {

  display.clearDisplay();

  drawHeader(
    "SYSTEM"
  );

  display.setCursor(
    0,
    18
  );

  display.print("CPU");

  display.setCursor(
    25,
    18
  );

  if (cpu < 10) {
    display.print(" ");
  }

  display.print(cpu);
  display.print("%");

  drawBar(
    55,
    18,
    72,
    8,
    cpu
  );

  display.setCursor(
    0,
    32
  );

  display.print("RAM");

  display.setCursor(
    25,
    32
  );

  if (ram < 10) {
    display.print(" ");
  }

  display.print(ram);
  display.print("%");

  drawBar(
    55,
    32,
    72,
    8,
    ram
  );

  display.setCursor(
    0,
    47
  );

  display.print("TEMP");

  display.setCursor(
    29,
    47
  );

  display.print(cpuTemp);
  display.print("C");

  display.setCursor(
    68,
    47
  );

  display.print("FAN");

  display.setCursor(
    91,
    47
  );

  if (fan >= 1000) {

    display.print(
      fan / 1000.0,
      1
    );

    display.print("K");

  } else {

    display.print(fan);
  }

  display.drawLine(
    0,
    61,
    127,
    61,
    SSD1306_WHITE
  );

  display.display();
}

// ============================================================
// GPU HISTORY
// ============================================================

void addGpuHistory(
  int value
) {

  gpuHistory[
    gpuHistoryIndex
  ] = constrain(
    value,
    0,
    100
  );

  if (
    ++gpuHistoryIndex >= HISTORY_SIZE
  ) {

    gpuHistoryIndex = 0;

    gpuHistoryFilled = true;
  }
}

// ============================================================
// GPU GRAPH
// ============================================================

void drawGpuGraph() {

  const int top = 43;
  const int bottom = 62;

  display.drawLine(
    0,
    bottom,
    127,
    bottom,
    SSD1306_WHITE
  );

  int count =
    gpuHistoryFilled
    ? HISTORY_SIZE
    : gpuHistoryIndex;

  for (
    int i = 1;
    i < count;
    i++
  ) {

    int a =
      gpuHistoryFilled
      ? (
          gpuHistoryIndex +
          i - 1
        ) % HISTORY_SIZE
      : i - 1;

    int b =
      gpuHistoryFilled
      ? (
          gpuHistoryIndex +
          i
        ) % HISTORY_SIZE
      : i;

    display.drawLine(

      (i - 1) * 2,

      map(
        gpuHistory[a],
        0,
        100,
        bottom - 1,
        top
      ),

      i * 2,

      map(
        gpuHistory[b],
        0,
        100,
        bottom - 1,
        top
      ),

      SSD1306_WHITE
    );
  }
}

// ============================================================
// GPU PAGE
// ============================================================

void drawGpuPage() {

  display.clearDisplay();

  drawHeader(
    "GPU // AMD"
  );

  display.setCursor(
    0,
    18
  );

  display.print("LOAD");

  display.setCursor(
    30,
    18
  );

  if (gpu < 10) {
    display.print(" ");
  }

  display.print(gpu);
  display.print("%");

  drawBar(
    59,
    18,
    68,
    8,
    gpu
  );

  display.setCursor(
    0,
    31
  );

  display.print("TEMP");

  display.setCursor(
    30,
    31
  );

  display.print(gpuTemp);
  display.print("C");

  display.setCursor(
    78,
    31
  );

  display.print("HISTORY");

  drawGpuGraph();

  display.display();
}

// ============================================================
// UPTIME
// ============================================================

void drawUptime() {

  unsigned long mins =
    hostUptime / 60;

  unsigned long hours =
    mins / 60;

  unsigned int m =
    mins % 60;

  if (hours >= 100) {

    display.print(
      hours / 24
    );

    display.print("d");

    display.print(
      hours % 24
    );

    display.print("h");

  } else {

    display.print(hours);
    display.print("h");

    if (m < 10) {
      display.print("0");
    }

    display.print(m);
  }
}

// ============================================================
// STORAGE
// ============================================================

void drawStoragePage() {

  display.clearDisplay();

  drawHeader(
    "STORAGE"
  );

  display.setCursor(
    0,
    18
  );

  display.print("SSD");

  display.setCursor(
    25,
    18
  );

  if (diskPercent < 10) {
    display.print(" ");
  }

  display.print(
    diskPercent
  );

  display.print("%");

  drawBar(
    55,
    18,
    72,
    8,
    diskPercent
  );

  display.setCursor(
    0,
    33
  );

  display.print("NVME");

  display.setCursor(
    30,
    33
  );

  display.print(
    nvmeTemp
  );

  display.print("C");

  display.setCursor(
    68,
    33
  );

  display.print("BAT");

  display.setCursor(
    91,
    33
  );

  display.print(
    battery
  );

  display.print("%");

  display.setCursor(
    0,
    48
  );

  display.print(
    plugged
    ? "AC"
    : "BAT"
  );

  display.setCursor(
    42,
    48
  );

  display.print("UP ");

  drawUptime();

  display.drawLine(
    0,
    61,
    127,
    61,
    SSD1306_WHITE
  );

  display.display();
}

// ============================================================
// NETWORK HISTORY
// ============================================================

void addNetworkHistory(
  unsigned long bps
) {

  unsigned long kb =
    bps / 1024UL;

  if (kb > 65535) {
    kb = 65535;
  }

  netHistory[
    netHistoryIndex
  ] = (uint16_t)kb;

  if (
    ++netHistoryIndex >= HISTORY_SIZE
  ) {

    netHistoryIndex = 0;

    netHistoryFilled = true;
  }
}

// ============================================================
// NETWORK SPEED
// ============================================================

void printNetworkSpeed(
  unsigned long bps
) {

  if (
    bps >= 1048576UL
  ) {

    display.print(
      bps / 1048576.0,
      1
    );

    display.print("M");

  } else if (
    bps >= 1024UL
  ) {

    display.print(
      bps / 1024UL
    );

    display.print("K");

  } else {

    display.print(bps);
    display.print("B");
  }
}

// ============================================================
// NETWORK GRAPH
// ============================================================

void drawNetworkGraph() {

  const int top = 42;
  const int bottom = 62;

  display.drawLine(
    0,
    bottom,
    127,
    bottom,
    SSD1306_WHITE
  );

  int count =
    netHistoryFilled
    ? HISTORY_SIZE
    : netHistoryIndex;

  if (count < 2) {
    return;
  }

  uint16_t maxValue = 1;

  for (
    int i = 0;
    i < count;
    i++
  ) {

    int idx =
      netHistoryFilled
      ? (
          netHistoryIndex + i
        ) % HISTORY_SIZE
      : i;

    if (
      netHistory[idx] > maxValue
    ) {

      maxValue =
        netHistory[idx];
    }
  }

  for (
    int i = 1;
    i < count;
    i++
  ) {

    int a =
      netHistoryFilled
      ? (
          netHistoryIndex +
          i - 1
        ) % HISTORY_SIZE
      : i - 1;

    int b =
      netHistoryFilled
      ? (
          netHistoryIndex +
          i
        ) % HISTORY_SIZE
      : i;

    display.drawLine(

      (i - 1) * 2,

      map(
        netHistory[a],
        0,
        maxValue,
        bottom - 1,
        top
      ),

      i * 2,

      map(
        netHistory[b],
        0,
        maxValue,
        bottom - 1,
        top
      ),

      SSD1306_WHITE
    );
  }
}

// ============================================================
// NETWORK PAGE
// ============================================================

void drawNetworkPage() {

  display.clearDisplay();

  drawHeader(
    "NETWORK"
  );

  display.setCursor(
    0,
    18
  );

  display.print("DOWN");

  display.setCursor(
    34,
    18
  );

  printNetworkSpeed(
    downloadBps
  );

  display.setCursor(
    92,
    18
  );

  display.print("/s");

  display.setCursor(
    0,
    30
  );

  display.print("UP");

  display.setCursor(
    34,
    30
  );

  printNetworkSpeed(
    uploadBps
  );

  display.setCursor(
    92,
    30
  );

  display.print("/s");

  drawNetworkGraph();

  display.display();
}

// ============================================================
// DEV V6
// ============================================================

void drawWakaTime() {

  int hours =
    wakaMinutes / 60;

  int minutes =
    wakaMinutes % 60;

  if (hours > 0) {

    display.print(hours);
    display.print("h");

    if (minutes > 0) {

      if (minutes < 10) {
        display.print("0");
      }

      display.print(minutes);
    }

  } else {

    display.print(minutes);
    display.print("m");
  }
}


// ============================================================
// DEV PAGE V6
// ============================================================

void drawDevPage() {

  display.clearDisplay();

  drawHeader(
    "DEV // TODAY"
  );

  // --------------------
  // CODE
  // --------------------

  display.setCursor(
    0,
    18
  );

  display.print("CODE");

  display.setCursor(
    34,
    18
  );

  drawWakaTime();


  // --------------------
  // PROJECT
  // --------------------

  display.setCursor(
    0,
    31
  );

  display.print(
    devProject
  );


  // --------------------
  // BRANCH
  // --------------------

  display.setCursor(
    0,
    44
  );

  display.print(
    devBranch
  );


  // --------------------
  // MOD
  // --------------------

  display.setCursor(
    88,
    44
  );

  display.print(
    gitModified
  );

  display.print("M");


  // --------------------
  // FOOTER
  // --------------------

  display.drawLine(
    0,
    61,
    127,
    61,
    SSD1306_WHITE
  );

  display.display();
}

// ============================================================
// SERIAL PARSER V6
// ============================================================

void parseMessage(
  String msg
) {

  int c =
    msg.indexOf("C");

  int r =
    msg.indexOf("|R");

  int t =
    msg.indexOf("|T");

  int f =
    msg.indexOf("|F");

  int g =
    msg.indexOf("|G");

  int gt =
    msg.indexOf("|GT");

  int d =
    msg.indexOf("|D");

  int n =
    msg.indexOf("|N");

  int b =
    msg.indexOf("|B");

  int p =
    msg.indexOf("|P");

  int u =
    msg.indexOf("|U");

  int rx =
    msg.indexOf("|RX");

  int tx =
    msg.indexOf("|TX");

  // V6

  int wt =
    msg.indexOf("|WT");

  int pj =
    msg.indexOf("|PJ");

  int br =
    msg.indexOf("|BR");

  int gm =
    msg.indexOf("|GM");


  if (
    c < 0 ||
    r < 0 ||
    t < 0 ||
    f < 0 ||
    g < 0 ||
    gt < 0 ||
    d < 0 ||
    n < 0 ||
    b < 0 ||
    p < 0 ||
    u < 0 ||
    rx < 0 ||
    tx < 0
  ) {

    return;
  }


  // ========================================================
  // V5 FIELDS
  // ========================================================

  cpu =
    msg.substring(
      c + 1,
      r
    ).toInt();

  ram =
    msg.substring(
      r + 2,
      t
    ).toInt();

  cpuTemp =
    msg.substring(
      t + 2,
      f
    ).toInt();

  fan =
    msg.substring(
      f + 2,
      g
    ).toInt();

  gpu =
    msg.substring(
      g + 2,
      gt
    ).toInt();

  gpuTemp =
    msg.substring(
      gt + 3,
      d
    ).toInt();

  diskPercent =
    msg.substring(
      d + 2,
      n
    ).toInt();

  nvmeTemp =
    msg.substring(
      n + 2,
      b
    ).toInt();

  battery =
    msg.substring(
      b + 2,
      p
    ).toInt();

  plugged =
    msg.substring(
      p + 2,
      u
    ).toInt();

  hostUptime =
    msg.substring(
      u + 2,
      rx
    ).toInt();


  // ========================================================
  // NETWORK
  // ========================================================

  if (wt >= 0) {

    downloadBps =
      strtoul(
        msg.substring(
          rx + 3,
          tx
        ).c_str(),
        NULL,
        10
      );

    uploadBps =
      strtoul(
        msg.substring(
          tx + 3,
          wt
        ).c_str(),
        NULL,
        10
      );

  } else {

    // Compatibilidad V5

    downloadBps =
      strtoul(
        msg.substring(
          rx + 3,
          tx
        ).c_str(),
        NULL,
        10
      );

    uploadBps =
      strtoul(
        msg.substring(
          tx + 3
        ).c_str(),
        NULL,
        10
      );
  }


  // ========================================================
  // DEV V6
  // ========================================================

  if (
    wt >= 0 &&
    pj >= 0 &&
    br >= 0 &&
    gm >= 0
  ) {

    wakaMinutes =
      msg.substring(
        wt + 3,
        pj
      ).toInt();


    devProject =
      msg.substring(
        pj + 3,
        br
      );


    devBranch =
      msg.substring(
        br + 3,
        gm
      );


    gitModified =
      msg.substring(
        gm + 3
      ).toInt();


    // underscores -> espacios

    devProject.replace(
      "_",
      " "
    );

    devBranch.replace(
      "_",
      " "
    );


    // Limites OLED

    if (
      devProject.length() > 20
    ) {

      devProject =
        devProject.substring(
          0,
          20
        );
    }


    if (
      devBranch.length() > 13
    ) {

      devBranch =
        devBranch.substring(
          0,
          13
        );
    }
  }


  // ========================================================
  // SAFETY
  // ========================================================

  cpu =
    constrain(
      cpu,
      0,
      100
    );

  ram =
    constrain(
      ram,
      0,
      100
    );

  cpuTemp =
    constrain(
      cpuTemp,
      0,
      150
    );

  fan =
    constrain(
      fan,
      0,
      10000
    );

  gpu =
    constrain(
      gpu,
      0,
      100
    );

  gpuTemp =
    constrain(
      gpuTemp,
      0,
      150
    );

  diskPercent =
    constrain(
      diskPercent,
      0,
      100
    );

  nvmeTemp =
    constrain(
      nvmeTemp,
      0,
      150
    );

  battery =
    constrain(
      battery,
      0,
      100
    );

  plugged =
    constrain(
      plugged,
      0,
      1
    );

  wakaMinutes =
    constrain(
      wakaMinutes,
      0,
      1440
    );

  gitModified =
    constrain(
      gitModified,
      0,
      999
    );


  // ========================================================
  // HISTORY / ALERTS
  // ========================================================

  addGpuHistory(
    gpu
  );

  addNetworkHistory(
    downloadBps
  );

  updateAlertLevel();

  lastData =
    millis();
}

// ============================================================
// BOOT SCREEN
// ============================================================

void drawBootScreen() {

  display.clearDisplay();

  display.setTextColor(
    SSD1306_WHITE
  );

  display.setTextSize(1);

  display.setCursor(
    0,
    0
  );

  display.print(
    "LINUX // MONITOR"
  );

  display.setCursor(
    27,
    25
  );

  display.print(
    "ESP32 V6 READY"
  );

  display.setCursor(
    13,
    42
  );

  display.print(
    "Waiting host..."
  );

  display.display();
}

// ============================================================
// SETUP
// ============================================================

void setup() {

  pinMode(
    LED_PIN,
    OUTPUT
  );

  digitalWrite(
    LED_PIN,
    LOW
  );

  Serial.begin(
    115200
  );

  Wire.begin(
    OLED_SDA,
    OLED_SCL
  );

  if (
    !display.begin(
      SSD1306_SWITCHCAPVCC,
      OLED_ADDR
    )
  ) {

    while (true) {
      delay(1000);
    }
  }

  for (
    int i = 0;
    i < HISTORY_SIZE;
    i++
  ) {

    gpuHistory[i] = 0;
    netHistory[i] = 0;
  }

  drawBootScreen();

  lastPageChange =
    millis();
}

// ============================================================
// LOOP
// ============================================================

void loop() {

  if (
    Serial.available()
  ) {

    String msg =
      Serial.readStringUntil(
        '\n'
      );

    msg.trim();

    if (
      msg.length() > 0
    ) {

      parseMessage(
        msg
      );
    }
  }


  // ========================================================
  // LED V5
  // ========================================================

  updateAlertLed();


  // ========================================================
  // PAGE ROTATION V6
  // ========================================================

  if (
    millis() -
    lastPageChange
    >= PAGE_TIME
  ) {

    lastPageChange =
      millis();

    currentPage++;

    // V5 era >3
    // V6 ahora tiene pagina 4 DEV

    if (
      currentPage > 4
    ) {

      currentPage = 0;
    }
  }


  // ========================================================
  // OLED
  // ========================================================

  if (
    millis() -
    lastRefresh
    >= 250
  ) {

    lastRefresh =
      millis();

    switch (
      currentPage
    ) {

      case 0:

        drawSystemPage();

        break;


      case 1:

        drawGpuPage();

        break;


      case 2:

        drawStoragePage();

        break;


      case 3:

        drawNetworkPage();

        break;


      case 4:

        drawDevPage();

        break;
    }
  }
}
