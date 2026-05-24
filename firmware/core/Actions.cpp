// --- Routing actions (schedule, GPIO, HTTP callbacks) ---
#include <Arduino.h>
#include "Actions.h"
#include "actions_logic.h"
#include "EEPROM.h"
#include <WiFiClient.h>

static ActionScheduleConfig action_schedule_config(const Action &a) {
  ActionScheduleConfig cfg;
  cfg.period_count = a.periodCount;
  for (uint8_t i = 0; i < a.periodCount && i < 8; i++) {
    cfg.periods[i].type = a.Type[i];
    cfg.periods[i].period_start = a.period_start[i];
    cfg.periods[i].period_end = a.period_end[i];
    cfg.periods[i].power_min = a.power_min[i];
    cfg.periods[i].power_max = a.power_max[i];
    cfg.periods[i].temp_min = a.temp_min[i];
    cfg.periods[i].temp_max = a.temp_max[i];
  }
  return cfg;
}



//Class Action
Action::Action() {
  valide = false;
  GpioOn = -1;
  GpioOff = -1;
  OutOn = 0;
  OutOff = 0;
  Kp = 0;
  Ki = 4;
  Kd = 0;
  PID = false;
  ClearOverride();
}
Action::Action(int aIdx) {
  valide = true;  // invalid pin index: no-op
  Idx = aIdx;
  T_LastAction = int(millis() / 1000);
  On = false;
  GpioOn = -1;
  GpioOff = -1;
  OutOn = 0;
  OutOff = 0;
  Kp = 0;
  Ki = 4;
  Kd = 0;
  PID = false;
  ClearOverride();
}

bool Action::OverrideExpired(unsigned long nowMs) {
  return actions_logic_override_expired(OverrideState, OverrideUntilMillis, nowMs);
}

void Action::ClearOverride() {
  OverrideState = ACTION_OVERRIDE_AUTO;
  OverrideTriacPercent = 0;
  OverrideUntilMillis = 0;
  LastOverrideState = ACTION_OVERRIDE_AUTO;
}

void Action::SetOverride(byte state, byte triacPercent, unsigned long durationSec) {
  OverrideState = state;
  OverrideTriacPercent = min((byte)100, triacPercent);
  OverrideUntilMillis = durationSec == 0 ? 0 : millis() + durationSec * 1000UL;
  LastOverrideState = ACTION_OVERRIDE_AUTO;
}


void Action::apply_regulation(float Pw, int wall_decihours, float Temperature) {
  bool TemperatureOk = false;
  int Tseconde = int(millis() / 1000);
  unsigned long nowMs = millis();
  if (OverrideExpired(nowMs)) ClearOverride();
  if (Host == "localhost" && (GpioOff < 0 || GpioOff > 33 || GpioOn < 0 || GpioOn > 33)) valide = false;
  if (valide && Idx > 0 && (OverrideState == ACTION_OVERRIDE_ON || OverrideState == ACTION_OVERRIDE_OFF)) {
    bool wantOn = OverrideState == ACTION_OVERRIDE_ON;
    if (Host == "localhost") {
      if (wantOn) {
        pinMode(GpioOn, OUTPUT);
        digitalWrite(GpioOn, OutOn);
      } else {
        pinMode(GpioOff, OUTPUT);
        digitalWrite(GpioOff, OutOff);
      }
      On = wantOn;
      T_LastAction = Tseconde;
      LastOverrideState = OverrideState;
      return;
    }
    if (On != wantOn || LastOverrideState != OverrideState) {
      CallExterne(Host, wantOn ? path_on : path_off, Port);
      On = wantOn;
      T_LastAction = Tseconde;
      LastOverrideState = OverrideState;
    }
    return;
  }
  LastOverrideState = ACTION_OVERRIDE_AUTO;
  if (valide && Idx > 0 && (Tseconde - T_LastAction) >= Tempo) {
    if (action_regulation_enabled(Actif) && Actif == kModeDecoupeOnoff) {
      if (Host == "localhost") {
        for (int i = 0; i < periodCount; i++) {
          if (wall_decihours >= period_start[i] && wall_decihours < period_end[i]) {
            TemperatureOk = true;
            if (Temperature > -100) {
              if (temp_min[i] <= 100 && Temperature > temp_min[i]) { TemperatureOk = false; }
              if (temp_max[i] <= 100 && Temperature < temp_max[i]) { TemperatureOk = false; }
            }
            switch (Type[i]) {  //NO,OFF,ON,PW,Triac
              case 1:           //OFF
                digitalWrite(GpioOff, OutOff);
                On = false;
                T_LastAction = Tseconde;
                break;
              case 2:  //ON
                if (TemperatureOk) {
                  digitalWrite(GpioOn, OutOn);
                  On = true;
                  T_LastAction = Tseconde;
                } else {
                  digitalWrite(GpioOff, OutOff);
                  On = false;
                  T_LastAction = Tseconde;
                }
                break;
              case 3:
                if (Pw < power_min[i] && TemperatureOk) {
                  digitalWrite(GpioOn, OutOn);
                  On = true;
                  T_LastAction = Tseconde;
                }
                if (Pw > power_max[i] || !TemperatureOk) {
                  digitalWrite(GpioOff, OutOff);
                  On = false;
                  T_LastAction = Tseconde;
                }
                break;
            }
          }
        }
      } else {  //Ordre distant

        for (int i = 0; i < periodCount; i++) {
          if (wall_decihours >= period_start[i] && wall_decihours < period_end[i]) {
            
            TemperatureOk = true;
            if (Temperature > -100) {
              if (temp_min[i] <= 100 && Temperature > temp_min[i]) { TemperatureOk = false; }
              if (temp_max[i] <= 100 && Temperature < temp_max[i]) { TemperatureOk = false; }
            }
            switch (Type[i]) {  //NO,OFF,ON,PW,Triac
              case 1:           //OFF
                if (On) {
                  CallExterne(Host, path_off, Port);
                  On = false;
                  T_LastAction = Tseconde;
                }
                break;
              case 2:  //ON
                if (!On && TemperatureOk) {
                  CallExterne(Host, path_on, Port);
                  On = true;
                  T_LastAction = Tseconde;
                }
                if (On && !TemperatureOk) {
                  CallExterne(Host, path_off, Port);
                  On = false;
                  T_LastAction = Tseconde;
                }
                break;
              case 3:
                if (Pw < power_min[i] && TemperatureOk && !On) {
                  CallExterne(Host, path_on, Port);
                  On = true;
                  T_LastAction = Tseconde;
                }
                if (Pw > power_max[i] || !TemperatureOk) {
                  if (On) {
                    CallExterne(Host, path_off, Port);
                    On = false;
                    T_LastAction = Tseconde;
                  }
                }
                if ((Tseconde - T_LastAction) > Repet && Repet != 0) {  //Repetion ancien ordre
                  if (On) {
                    CallExterne(Host, path_on, Port);
                  } else {
                    CallExterne(Host, path_off, Port);
                  }
                  T_LastAction = Tseconde;
                }
                break;
            }
          }
        }
      }
    } else {
      if (Host == "localhost") {
        digitalWrite(GpioOff, OutOff);
      } else {
        if (On) {
          CallExterne(Host, path_off, Port);
        }
      }
      On = false;
    }
  }
}


void Action::parse_from_wire(String ligne) {
  valide = true;
  Serial.println(ligne);
  String RS = String((char)30);  //Record Separator
  Actif = byte(ligne.substring(0, ligne.indexOf(RS)).toInt());
  ligne = ligne.substring(ligne.indexOf(RS) + 1);
  title = ligne.substring(0, ligne.indexOf(RS));
  ligne = ligne.substring(ligne.indexOf(RS) + 1);
  Host = ligne.substring(0, ligne.indexOf(RS));
  ligne = ligne.substring(ligne.indexOf(RS) + 1);
  Port = ligne.substring(0, ligne.indexOf(RS)).toInt();
  ligne = ligne.substring(ligne.indexOf(RS) + 1);
  path_on = ligne.substring(0, ligne.indexOf(RS));
  ligne = ligne.substring(ligne.indexOf(RS) + 1);
  path_off = ligne.substring(0, ligne.indexOf(RS));
  ligne = ligne.substring(ligne.indexOf(RS) + 1);
  Repet = ligne.substring(0, ligne.indexOf(RS)).toInt();
  Repet = min(Repet, 32000);
  Repet = max(0, Repet);
  ligne = ligne.substring(ligne.indexOf(RS) + 1);
  Tempo = ligne.substring(0, ligne.indexOf(RS)).toInt();
  Tempo = min(Tempo, 32000);
  Tempo = max(0, Tempo);
  if (Repet > 0) {
    Repet = max(Tempo + 4, Repet);  // avoid schedule overlap
  }
  ligne = ligne.substring(ligne.indexOf(RS) + 1);
  periodCount = byte(ligne.substring(0, ligne.indexOf(RS)).toInt());
  ligne = ligne.substring(ligne.indexOf(RS) + 1);
  int period_start_ = 0;
  for (byte i = 0; i < periodCount; i++) {
    Type[i] = byte(ligne.substring(0, ligne.indexOf(RS)).toInt());  //NO,OFF,ON,PW,Triac
    ligne = ligne.substring(ligne.indexOf(RS) + 1);
    period_end[i] = ligne.substring(0, ligne.indexOf(RS)).toInt();
    period_start[i] = period_start_;
    period_start_ = period_end[i];
    ligne = ligne.substring(ligne.indexOf(RS) + 1);
    power_min[i] = ligne.substring(0, ligne.indexOf(RS)).toInt();
    ligne = ligne.substring(ligne.indexOf(RS) + 1);
    power_max[i] = ligne.substring(0, ligne.indexOf(RS)).toInt();
    ligne = ligne.substring(ligne.indexOf(RS) + 1);
    temp_min[i] = ligne.substring(0, ligne.indexOf(RS)).toInt();
    ligne = ligne.substring(ligne.indexOf(RS) + 1);
    temp_max[i] = ligne.substring(0, ligne.indexOf(RS)).toInt();
    ligne = ligne.substring(ligne.indexOf(RS) + 1);
  }
}
String Action::serialize_to_wire() {
  String GS = String((char)29);  //Group Separator
  String RS = String((char)30);  //Record Separator
  String S;
  S += String(Actif) + RS;
  S += title + RS;
  S += Host + RS;
  S += String(Port) + RS;
  S += path_on + RS;
  S += path_off + RS;
  S += String(Repet) + RS;
  S += String(Tempo) + RS;
  S += String(periodCount) + RS;
  for (byte i = 0; i < periodCount; i++) {
    S += String(Type[i]) + RS;
    S += String(period_end[i]) + RS;
    S += String(power_min[i]) + RS;
    S += String(power_max[i]) + RS;
    S += String(temp_min[i]) + RS;
    S += String(temp_max[i]) + RS;
  }
  return S + GS;
}



byte Action::schedule_type_at(int wall_decihours) {
  return actions_logic_active_type(action_schedule_config(*this), wall_decihours);
}

byte Action::current_triac_schedule_type(int wall_decihours, float Temperature) {
  return actions_logic_active_type_triac(action_schedule_config(*this), wall_decihours, Temperature);
}

int Action::threshold_min_at(int wall_decihours) {
  return actions_logic_threshold_min(action_schedule_config(*this), wall_decihours);
}

int Action::threshold_max_at(int wall_decihours) {
  return actions_logic_threshold_max(action_schedule_config(*this), wall_decihours);
}