#pragma once
// =====================================================================================
//  H2 PRO — веб-интерфейс мозга (главная страница), вынесен из H2_Brain_v5.ino
//  Отдаётся по GET /. Встроенный JS работает через WebSocket /ws и HTTP-эндпоинты
//  мозга (см. раздел HTTP в H2_Brain_v5.ino).
//  ⚡ v6.0: выбор драйвера электролизёра убран (только MOSFET), выводной
//  вентилятор на реле (/setfans), новая вкладка «Звук» (/snd/*): мелодии событий,
//  библиотека мелодий и импорт MIDI / RTTTL / аудио с подготовкой под зуммер прямо
//  в браузере (без внешних библиотек — устройство работает в офлайн-сети).
// =====================================================================================
#include <Arduino.h>

const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html lang="ru"><head>
<meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>H2 PRO v6.0</title>
<style>
  @import url('https://fonts.googleapis.com/css2?family=Share+Tech+Mono&family=Exo+2:wght@300;600;800&display=swap');
  :root{
    --bg:#080810;--card:#131326;--border:#1e1e3a;
    --accent:#00d4ff;--ok:#00ff88;--warn:#ffcc00;--danger:#ff3355;--dim:#3a3a5a;--text:#c8d0e0;
    --purple:#bb88ff;--temp:#ff8844;
  }
  /* Светлая тема */
  body.light{
    --bg:#eef1f7;--card:#ffffff;--border:#d0d6e2;
    --accent:#0077cc;--ok:#00a050;--warn:#cc8800;--danger:#cc2244;--dim:#99a0b0;--text:#1a2030;
    --purple:#7744bb;--temp:#dd5500;
  }
  *{box-sizing:border-box;margin:0;padding:0}
  body{background:var(--bg);color:var(--text);font-family:'Exo 2',sans-serif;
    min-height:100vh;padding:14px 10px;transition:background .3s,color .3s;
    background-image:radial-gradient(ellipse at 20% 0%,rgba(0,212,255,.07) 0%,transparent 60%),
      radial-gradient(ellipse at 80% 100%,rgba(77,138,255,.07) 0%,transparent 60%);}
  body.light{background-image:none;}
  .topbar{max-width:480px;margin:0 auto 10px;display:flex;justify-content:space-between;align-items:center;}
  h1{font-size:1.05em;font-weight:800;letter-spacing:5px;color:var(--accent);
    text-transform:uppercase;text-shadow:0 0 20px rgba(0,212,255,.5);}
  body.light h1{text-shadow:none;}
  .theme-btn{background:var(--card);border:1px solid var(--border);color:var(--text);
    border-radius:8px;padding:6px 10px;cursor:pointer;font-size:.9em;transition:all .2s;}
  .theme-btn:hover{border-color:var(--accent);}
  .ws-dot{display:inline-block;width:8px;height:8px;border-radius:50%;margin-left:6px;
    background:var(--dim);transition:all .3s;vertical-align:middle;}
  .ws-dot.live{background:var(--ok);box-shadow:0 0 8px var(--ok);}

  .panel{max-width:480px;margin:0 auto;display:flex;flex-direction:column;gap:10px;}

  /* Вкладки — ⚡ v6.0: шесть вкладок, сетка 3×2 (влезает на экран телефона) */
  .tabs{display:grid;grid-template-columns:repeat(3,1fr);gap:4px;background:var(--card);
    border:1px solid var(--border);border-radius:10px;padding:4px;}
  .tab{padding:8px 4px;border:none;background:transparent;color:var(--dim);
    border-radius:7px;cursor:pointer;font-family:'Exo 2',sans-serif;font-size:.7em;
    font-weight:600;letter-spacing:1px;text-transform:uppercase;transition:all .2s;}
  .tab.active{background:var(--accent);color:var(--bg);}
  .diag-row{display:flex;justify-content:space-between;align-items:center;gap:10px;
    padding:9px 2px;border-bottom:1px solid var(--border);font-size:.82em;}
  .diag-row:last-child{border-bottom:none;}
  .pill{font-size:.7em;font-weight:800;letter-spacing:1px;padding:3px 11px;border-radius:20px;
    border:1px solid var(--dim);color:var(--dim);white-space:nowrap;}
  .pill.ok{color:var(--ok);border-color:var(--ok);background:rgba(0,255,136,.08);}
  .pill.bad{color:var(--danger);border-color:var(--danger);background:rgba(255,51,85,.10);}
  .diag-hint{font-size:.8em;color:var(--text);line-height:1.6;}
  .page{display:none;flex-direction:column;gap:10px;}
  .page.active{display:flex;}

  .alert-bar{background:#1a1500;border:1px solid var(--warn);color:var(--warn);
    border-radius:10px;padding:8px 12px;text-align:center;font-size:.75em;
    letter-spacing:2px;text-transform:uppercase;font-family:'Share Tech Mono',monospace;display:none;}
  .alert-bar.show{display:block;animation:ap 1.5s infinite}
  .alert-bar.danger{background:#1a0008;border-color:var(--danger);color:var(--danger)}
  .alert-bar.tilt{background:#1a0a1a;border-color:#ff6bff;color:#ff6bff}
  @keyframes ap{0%,100%{opacity:.7}50%{opacity:1}}

  .pressure{background:var(--card);border:1px solid var(--border);border-radius:14px;
    padding:16px;text-align:center;position:relative;overflow:hidden;}
  .pressure::before{content:'';position:absolute;top:0;left:0;right:0;height:2px;
    background:linear-gradient(90deg,transparent,var(--accent),transparent);}
  .pressure.warn::before{background:linear-gradient(90deg,transparent,var(--warn),transparent)}
  .pressure.danger::before{background:linear-gradient(90deg,transparent,var(--danger),transparent)}
  .p-lbl{font-size:.65em;letter-spacing:4px;color:var(--dim);text-transform:uppercase;margin-bottom:6px}
  .p-num{font-family:'Share Tech Mono',monospace;font-size:3.4em;color:var(--accent);
    line-height:1;text-shadow:0 0 30px rgba(0,212,255,.6);transition:color .3s;}
  .p-num.warn{color:var(--warn)}.p-num.danger{color:var(--danger)}
  .p-unit{font-size:.4em;color:var(--dim);margin-left:6px}
  .p-bar-wrap{position:relative;margin-top:10px}
  .p-bar{height:6px;background:rgba(0,0,0,.3);border-radius:3px;overflow:hidden;position:relative;}
  body.light .p-bar{background:#e0e4ec;}
  .p-hyst{position:absolute;top:0;height:6px;background:rgba(255,204,0,.22);
    border-left:1px dashed rgba(255,204,0,.5);border-right:1px dashed rgba(255,204,0,.5);
    pointer-events:none;transition:all .3s;}
  .p-fill{height:100%;border-radius:3px;background:linear-gradient(90deg,var(--ok),var(--accent));
    transition:width .5s ease,background .3s;position:relative;z-index:1;}
  .p-fill.warn{background:linear-gradient(90deg,var(--accent),var(--warn))}
  .p-fill.danger{background:linear-gradient(90deg,var(--warn),var(--danger))}
  .p-tgt{position:absolute;top:-3px;width:2px;height:13px;background:var(--warn);
    border-radius:1px;transition:left .5s ease;pointer-events:none;z-index:2;}
  .p-scale{display:flex;justify-content:space-between;font-size:.55em;color:var(--dim);
    font-family:'Share Tech Mono',monospace;margin-top:4px;letter-spacing:1px}

  /* Температура + ⚡ v6.0: состояние двух вентиляторов (реле) */
  .temp-card{background:var(--card);border:1px solid var(--border);border-radius:12px;
    padding:12px 16px;display:flex;justify-content:space-between;align-items:center;}
  .temp-card::before{content:'';position:absolute;}
  .temp-left{display:flex;flex-direction:column;gap:2px;}
  .temp-lbl{font-size:.58em;letter-spacing:3px;color:var(--dim);text-transform:uppercase;}
  .temp-val{font-family:'Share Tech Mono',monospace;font-size:1.8em;color:var(--temp);line-height:1;}
  .temp-val.hot{color:var(--danger);animation:ap 1s infinite;}
  .fan-info{display:flex;flex-direction:column;gap:6px;font-size:.7em;
    font-family:'Share Tech Mono',monospace;color:var(--dim);}
  .fan-ln{display:flex;align-items:center;justify-content:flex-end;gap:7px;}
  .fan-ln b{font-weight:400;min-width:36px;text-align:right;}
  .fan-ln.on b{color:var(--accent);}
  .fan-spin{display:inline-block;}
  .fan-ln.on .fan-spin{color:var(--accent);animation:spin 1.6s linear infinite;}
  @keyframes spin{from{transform:rotate(0)}to{transform:rotate(360deg)}}

  .chart-box{background:var(--card);border:1px solid var(--border);border-radius:14px;padding:10px 12px;}
  .chart-hdr{display:flex;justify-content:space-between;align-items:center;margin-bottom:6px;}
  .chart-lbl{font-size:.6em;letter-spacing:2px;color:var(--dim);text-transform:uppercase;}
  .range-btns{display:flex;gap:3px;}
  .range-btn{padding:3px 8px;border:1px solid var(--border);border-radius:6px;background:transparent;
    color:var(--dim);font-family:'Share Tech Mono',monospace;font-size:.62em;cursor:pointer;transition:all .2s;}
  .range-btn.active{border-color:var(--accent);color:var(--accent);background:rgba(0,212,255,.1);}
  canvas{width:100%;display:block;}

  .tilt-row{display:flex;justify-content:space-between;align-items:center;
    background:var(--card);border:1px solid var(--border);border-radius:10px;
    padding:8px 12px;font-size:.65em;letter-spacing:2px;
    font-family:'Share Tech Mono',monospace;color:var(--dim);}
  .tilt-row.warn{border-color:var(--warn);color:var(--warn);}
  .tilt-row.danger{border-color:#ff6bff;color:#ff6bff;animation:pulse .9s infinite;}
  .tilt-val{color:var(--ok);font-size:1.2em;}
  .tilt-val.warn{color:var(--warn);}.tilt-val.danger{color:#ff6bff;}

  .badges{display:flex;flex-wrap:wrap;gap:6px}
  .badge{padding:4px 10px;border-radius:20px;font-size:.62em;font-weight:600;
    letter-spacing:1.5px;text-transform:uppercase;background:var(--card);color:var(--dim);
    border:1px solid var(--border);transition:all .25s;}
  .badge.ok{background:#001a0e;color:var(--ok);border-color:var(--ok);}
  body.light .badge.ok{background:#e0fff0;}
  .badge.warn{background:#1a1500;color:var(--warn);border-color:var(--warn);}
  body.light .badge.warn{background:#fff8e0;}
  .badge.danger{background:#1a0008;color:var(--danger);border-color:var(--danger);animation:pulse .9s infinite}
  .badge.tilt{background:#1a0a1a;color:#ff6bff;border-color:#ff6bff;animation:pulse .9s infinite}
  .badge.purple{background:#1a0a2a;color:var(--purple);border-color:var(--purple);animation:pulse .9s infinite}
  @keyframes pulse{0%,100%{box-shadow:0 0 6px rgba(255,51,85,.3)}50%{box-shadow:0 0 14px rgba(255,51,85,.7)}}

  .info-row{display:flex;justify-content:space-between;align-items:center;padding:0 4px;
    font-size:.62em;letter-spacing:2px;color:var(--dim);font-family:'Share Tech Mono',monospace;}
  .rf-indicator{display:flex;align-items:center;gap:6px;}
  .rf-dot{width:7px;height:7px;border-radius:50%;background:var(--dim);transition:all .3s}
  .rf-dot.ok{background:var(--ok);box-shadow:0 0 8px var(--ok)}
  .rf-dot.fail{background:var(--danger);animation:pulse .9s infinite}

  .btn-power{width:100%;padding:16px;border:none;border-radius:12px;cursor:pointer;
    font-family:'Exo 2',sans-serif;font-size:.9em;font-weight:800;letter-spacing:3px;
    text-transform:uppercase;transition:all .2s;
    background:var(--card);color:var(--text);border:1px solid var(--border);}
  .btn-power.on{background:linear-gradient(135deg,#1a0010,#330015);color:var(--danger);
    border-color:var(--danger);box-shadow:0 0 20px rgba(255,51,85,.2);}
  body.light .btn-power.on{background:#ffe0e6;}
  .btn-power:active{transform:scale(.97)}

  .grid{display:grid;grid-template-columns:1fr 1fr;gap:8px}
  .ctrl{padding:14px 8px;border:1px solid var(--border);border-radius:11px;
    background:var(--card);cursor:pointer;font-family:'Exo 2',sans-serif;
    font-size:.7em;font-weight:600;letter-spacing:1px;text-transform:uppercase;
    color:var(--text);transition:all .2s;display:flex;flex-direction:column;
    align-items:center;gap:8px;}
  .ctrl .dot{width:8px;height:8px;border-radius:50%;background:var(--dim);transition:all .25s}
  .ctrl.wide{grid-column:1/-1}
  .ctrl:hover:not(:disabled){filter:brightness(1.2)}
  .ctrl:active:not(:disabled){transform:scale(.95)}
  .ctrl:disabled{opacity:.25;cursor:not-allowed}
  .ctrl.active{background:#001a0e;border-color:var(--ok)}
  .ctrl.active .dot{background:var(--ok);box-shadow:0 0 8px var(--ok)}
  .ctrl.active-warn{background:#1a1500;border-color:var(--warn)}
  .ctrl.active-warn .dot{background:var(--warn);box-shadow:0 0 8px var(--warn)}
  .ctrl.active-red{background:#1a0008;border-color:var(--danger)}
  .ctrl.active-red .dot{background:var(--danger);box-shadow:0 0 8px var(--danger)}
  .ctrl.active-purple{background:#1a0a2a;border-color:var(--purple)}
  .ctrl.active-purple .dot{background:var(--purple);box-shadow:0 0 8px var(--purple)}
  .ctrl.purge-btn{background:#0a1a0a;border-color:#2a4a2a;color:#8f8;}
  .ctrl.repulsor-btn{background:#150a2a;border-color:#3a2a5a;color:var(--purple);}

  .footer{text-align:center;font-size:.7em;letter-spacing:2px;color:var(--warn);
    min-height:16px;font-family:'Share Tech Mono',monospace;}

  .block{background:var(--card);border:1px solid var(--border);border-radius:12px;
    padding:14px;position:relative;overflow:hidden;}
  .block::before{content:'';position:absolute;top:0;left:0;right:0;height:2px;}
  .block.green::before{background:linear-gradient(90deg,transparent,#56d364,transparent);}
  .block.warn::before{background:linear-gradient(90deg,transparent,var(--warn),transparent);}
  .block.temp::before{background:linear-gradient(90deg,transparent,var(--temp),transparent);}
  .blk-lbl{font-size:.62em;letter-spacing:4px;color:var(--dim);text-transform:uppercase;
    text-align:center;margin-bottom:10px;}
  /* ⚡ v6.0: заголовок карточки с состоянием справа */
  .blk-head{display:flex;justify-content:space-between;align-items:center;gap:8px;margin-bottom:12px;}
  .blk-head .blk-lbl{margin:0;text-align:left;}
  .blk-aux{font-family:'Share Tech Mono',monospace;font-size:.62em;letter-spacing:1px;color:var(--dim);white-space:nowrap;}

  .seg{display:flex;gap:6px;margin-bottom:10px;}
  .seg-opt{flex:1;padding:9px 4px;border:1px solid var(--border);border-radius:8px;
    background:transparent;cursor:pointer;font-family:'Exo 2',sans-serif;
    font-size:.7em;font-weight:600;letter-spacing:1px;text-transform:uppercase;
    color:var(--dim);text-align:center;transition:all .2s;}
  .seg-opt.active{border-color:var(--accent);color:var(--accent);background:rgba(0,212,255,.1);}

  .row{display:flex;align-items:center;gap:8px;margin-bottom:8px;}
  .row label{font-size:.6em;letter-spacing:2px;color:var(--dim);text-transform:uppercase;min-width:62px;}
  .row.lw label{min-width:104px;}
  .row .val{font-family:'Share Tech Mono',monospace;font-size:.95em;color:var(--accent);min-width:70px;text-align:right;}
  input[type=range]{flex:1;-webkit-appearance:none;height:4px;border-radius:2px;outline:none;cursor:pointer;
    background:linear-gradient(90deg,var(--accent) var(--pct,0%),var(--border) var(--pct,0%));}
  input[type=range]::-webkit-slider-thumb{-webkit-appearance:none;width:16px;height:16px;
    border-radius:50%;background:var(--accent);cursor:pointer;}
  input[type=range]:disabled{opacity:.4;}

  .t-pm{width:38px;height:38px;border-radius:9px;border:1px solid var(--warn);
    background:rgba(255,204,0,.1);color:var(--warn);font-size:1.3em;font-weight:800;
    cursor:pointer;display:flex;align-items:center;justify-content:center;transition:all .2s;}
  .t-pm:active{transform:scale(.92);}
  .t-val{font-family:'Share Tech Mono',monospace;font-size:1.9em;color:var(--warn);min-width:88px;text-align:center;}
  .t-unit{font-size:.42em;color:var(--dim);}
  .qrow{display:flex;gap:5px;margin-bottom:10px;}
  .qb{flex:1;padding:6px 2px;border:1px solid var(--border);border-radius:7px;background:transparent;
    cursor:pointer;font-family:'Share Tech Mono',monospace;font-size:.7em;color:var(--dim);transition:all .2s;text-align:center;}
  .qb.active{border-color:var(--warn);color:var(--warn);background:rgba(255,204,0,.1);}
  .apply-btn{width:100%;padding:10px;border:1px solid var(--accent);border-radius:9px;
    background:rgba(0,212,255,.1);color:var(--accent);cursor:pointer;font-family:'Exo 2',sans-serif;
    font-size:.72em;font-weight:800;letter-spacing:2px;text-transform:uppercase;transition:all .2s;}
  .apply-btn:active{transform:scale(.97);}
  .ok-msg{text-align:center;font-size:.6em;font-family:'Share Tech Mono',monospace;
    color:var(--ok);min-height:14px;margin-top:5px;transition:opacity .4s;opacity:0;}

  /* Статистика */
  .stat-grid{display:grid;grid-template-columns:1fr 1fr;gap:8px;}
  .stat-item{background:var(--card);border:1px solid var(--border);border-radius:10px;padding:10px 12px;
    display:flex;flex-direction:column;justify-content:space-between;}
  .stat-k{font-size:.55em;letter-spacing:2px;color:var(--dim);text-transform:uppercase;margin-bottom:3px;}
  .stat-v{font-family:'Share Tech Mono',monospace;font-size:1.2em;color:var(--accent);}
  .stat-v.sm{font-size:.95em;}

  .data-btns{display:grid;grid-template-columns:1fr 1fr;gap:8px;}
  .data-btn{padding:11px;border:1px solid var(--border);border-radius:9px;background:var(--card);
    color:var(--text);cursor:pointer;font-family:'Exo 2',sans-serif;font-size:.66em;font-weight:600;
    letter-spacing:1px;text-transform:uppercase;transition:all .2s;}
  .data-btn:hover{border-color:var(--accent);color:var(--accent);}
  .data-btn:active{transform:scale(.96);}
  .data-note{font-size:.58em;color:var(--dim);text-align:center;line-height:1.6;
    font-family:'Share Tech Mono',monospace;margin-top:4px;}

  .ota-link{display:block;text-align:center;font-size:.62em;letter-spacing:2px;
    color:#4d8aff;text-decoration:none;padding:8px;border:1px dashed #1e3a5a;
    border-radius:8px;transition:all .2s;font-family:'Share Tech Mono',monospace;text-transform:uppercase;}
  .ota-link:hover{background:rgba(77,138,255,.1);border-color:#4d8aff;}
  .ota-link.disabled{color:var(--dim);border-color:var(--border);cursor:not-allowed;}

  /* ⚡ v6.0: общие элементы форм (вентиляторы, звук) */
  .apply-btn:disabled,.data-btn:disabled,.ico-btn:disabled{opacity:.35;cursor:not-allowed;transform:none;}
  .sub-lbl{font-size:.6em;letter-spacing:2px;color:var(--dim);text-transform:uppercase;margin-bottom:6px;}
  .hint{font-size:.72em;color:var(--dim);line-height:1.5;margin-bottom:10px;}
  .fld{margin-bottom:12px;}
  .inp{width:100%;padding:9px 10px;border:1px solid var(--border);border-radius:8px;background:transparent;
    color:var(--text);font-family:'Exo 2',sans-serif;font-size:.85em;outline:none;transition:border-color .2s;}
  .inp:focus{border-color:var(--accent);}
  select.inp{-webkit-appearance:none;appearance:none;padding-right:30px;cursor:pointer;
    background-image:linear-gradient(45deg,transparent 50%,var(--dim) 50%),linear-gradient(135deg,var(--dim) 50%,transparent 50%);
    background-position:calc(100% - 16px) 50%,calc(100% - 11px) 50%;background-size:5px 5px;background-repeat:no-repeat;}
  select.inp option{background:var(--card);color:var(--text);}
  textarea.inp{font-family:'Share Tech Mono',monospace;font-size:.75em;resize:vertical;min-height:62px;line-height:1.4;}
  .row select.inp{flex:1;width:auto;}
  @supports (-webkit-touch-callout:none){ .inp,textarea.inp{font-size:16px;} }  /* iOS: без автозума */
  .sw{-webkit-appearance:none;appearance:none;flex:none;width:36px;height:20px;border-radius:10px;
    border:1px solid var(--border);background:transparent;position:relative;cursor:pointer;transition:all .2s;}
  .sw::after{content:'';position:absolute;top:3px;left:3px;width:12px;height:12px;border-radius:50%;
    background:var(--dim);transition:all .2s;}
  .sw:checked{border-color:var(--accent);background:rgba(0,212,255,.12);}
  .sw:checked::after{left:19px;background:var(--accent);}
  .opt-list{border-top:1px solid var(--border);margin-bottom:12px;}
  .opt-list .diag-row{cursor:pointer;}
  .btn-row{display:grid;grid-template-columns:2fr 1fr 1fr;gap:8px;}
  .btn-row.two{grid-template-columns:1fr 1fr;}
  .btn-row>button{padding:10px 6px;}

  /* ⚡ v6.0: сворачиваемые секции */
  .fold>summary{list-style:none;cursor:pointer;display:flex;justify-content:space-between;align-items:center;
    -webkit-tap-highlight-color:transparent;outline:none;}
  .fold>summary::-webkit-details-marker{display:none;}
  .fold>summary::after{content:'';width:7px;height:7px;border-right:1.5px solid var(--dim);
    border-bottom:1.5px solid var(--dim);transform:rotate(45deg);transition:transform .2s;margin:0 4px 4px 0;}
  .fold[open]>summary::after{transform:rotate(-135deg);margin:4px 4px 0 0;}
  .fold>summary .blk-lbl{margin:0;text-align:left;}
  .fold:not(.sub)>summary .blk-lbl{color:var(--accent);}
  .fold:not(.sub)>summary::after{border-color:var(--accent);}
  .fold[open]>summary{margin-bottom:14px;}
  .fold.sub{border-top:1px solid var(--border);padding-top:11px;margin:2px 0 12px;}
  .fold.sub>summary{font-size:.6em;letter-spacing:2px;color:var(--dim);text-transform:uppercase;}
  .fold.sub[open]>summary{margin-bottom:12px;}

  /* ⚡ v6.0: библиотека мелодий */
  .mel-row{display:flex;align-items:center;gap:6px;padding:9px 0;border-bottom:1px solid var(--border);}
  .mel-row:last-child{border-bottom:none;}
  .mel-main{flex:1;min-width:0;}
  .mel-name{font-size:.82em;white-space:nowrap;overflow:hidden;text-overflow:ellipsis;}
  .mel-meta{font-size:.62em;color:var(--dim);font-family:'Share Tech Mono',monospace;margin-top:2px;}
  .mel-empty{font-size:.75em;color:var(--dim);text-align:center;padding:10px 0 12px;
    border-bottom:1px solid var(--border);}
  .ico-btn{flex:none;width:34px;height:34px;display:inline-flex;align-items:center;justify-content:center;
    border:1px solid var(--border);border-radius:8px;background:transparent;color:var(--dim);
    cursor:pointer;transition:all .2s;}
  .ico-btn svg{width:15px;height:15px;fill:none;stroke:currentColor;stroke-width:2;
    stroke-linecap:round;stroke-linejoin:round;}
  .ico-btn:hover,.ico-btn.on{border-color:var(--accent);color:var(--accent);}
  .ico-btn.del:hover{border-color:var(--danger);color:var(--danger);}
  .ico-btn:active{transform:scale(.94);}
  .ico-ph{flex:none;width:34px;}

  /* ⚡ v6.0: импорт мелодии */
  .file-pick{display:flex;align-items:center;gap:10px;margin-bottom:12px;}
  .file-pick .data-btn{flex:none;}
  .file-name{font-size:.75em;color:var(--dim);min-width:0;overflow:hidden;text-overflow:ellipsis;white-space:nowrap;}
  .prog{height:3px;background:var(--border);border-radius:2px;overflow:hidden;margin-top:10px;display:none;}
  .prog.show{display:block;}
  .prog>i{display:block;height:100%;width:0;background:var(--accent);transition:width .15s;}
  .msg{font-size:.72em;min-height:16px;margin-top:8px;color:var(--dim);line-height:1.5;}
  .msg.ok{color:var(--ok);}
  .msg.bad{color:var(--danger);}
  .mel-res{display:none;margin-top:12px;padding-top:12px;border-top:1px solid var(--border);}
  .mel-res.show{display:block;}
  .mel-stat{font-family:'Share Tech Mono',monospace;font-size:.85em;color:var(--accent);margin-bottom:4px;}
  .mel-tips{margin-top:14px;padding-top:12px;border-top:1px solid var(--border);}
  .mel-tips p{font-size:.7em;color:var(--dim);line-height:1.55;}
  .mel-tips p+p{margin-top:8px;}
</style></head>
<body>
<div class="topbar">
  <h1>⚡ H2 PRO<span class="ws-dot" id="wsDot"></span></h1>
  <button class="theme-btn" id="themeBtn" onclick="toggleTheme()">🌙</button>
</div>

<div class="panel">
  <div class="tabs">
    <button class="tab active" onclick="showPage('main',this)">Управление</button>
    <button class="tab" onclick="showPage('graph',this)">Графики</button>
    <button class="tab" onclick="showPage('settings',this)">Настройки</button>
    <button class="tab" onclick="showPage('stats',this)">Данные</button>
    <button class="tab" onclick="showPage('sound',this)">Звук</button>
    <button class="tab" onclick="showPage('diag',this)">Диагн.</button>
  </div>

  <!-- ════════ СТРАНИЦА: УПРАВЛЕНИЕ ════════ -->
  <div class="page active" id="page-main">
    <div class="alert-bar" id="alertBar"></div>

    <div class="pressure" id="pBox">
      <div class="p-lbl">Давление в баке</div>
      <div class="p-num" id="pNum"><span id="pv">0.00</span><span class="p-unit">бар</span></div>
      <div class="p-bar-wrap">
        <div class="p-bar">
          <div class="p-hyst" id="physt" style="left:16.6%;width:0%"></div>
          <div class="p-fill" id="pf" style="width:0%"></div>
        </div>
        <div class="p-tgt" id="ptgt" style="left:16.6%"></div>
        <div class="p-scale"><span>0</span><span>2</span><span>4</span><span>6 бар</span></div>
      </div>
    </div>

    <div class="temp-card block temp">
      <div class="temp-left">
        <div class="temp-lbl">Температура</div>
        <div class="temp-val" id="tempVal">--°</div>
      </div>
      <div class="fan-info">
        <div class="fan-ln" id="fanL1"><span class="fan-spin">❄</span>корпус<b>питание напрямую</b></div>
        <div class="fan-ln" id="fanL2"><span class="fan-spin">❄</span>выводной<b id="fanTx2">—</b></div>
      </div>
    </div>

    <div class="tilt-row" id="tiltRow">
      <span id="tiltLbl">НАКЛОН (порог 30°)</span>
      <span class="tilt-val" id="tiltVal">--°</span>
    </div>

    <div class="badges">
      <span class="badge" id="bPwr">POWER</span>
      <span class="badge" id="bLck">LOCK</span>
      <span class="badge" id="bTlt">TILT</span>
      <span class="badge" id="bPrg">PURGE</span>
      <span class="badge" id="bPsh">PURGE SHORT</span>
      <span class="badge" id="bRep">РЕПУЛЬСОР</span>
      <span class="badge" id="bMgs">MANUAL</span>
      <span class="badge" id="bPzo">PIEZO</span>
      <span class="badge" id="bEle">ELECTRO</span>
      <span class="badge" id="bVal">VALVE</span>
    </div>

    <div class="info-row">
      <div class="rf-indicator"><div class="rf-dot" id="rfDot"></div><span id="rfTxt">ПУЛЬТ: —</span></div>
      <div id="uptime">UP 00:00:00</div>
    </div>

    <button class="btn-power" id="btnPwr" onclick="handlePower()">ВКЛЮЧИТЬ СИСТЕМУ</button>

    <div class="grid">
      <button class="ctrl wide" id="cLck" onclick="cmd(2)"><div class="dot"></div>БЛОКИРОВКА / СБРОС АВАРИЙ</button>
      <button class="ctrl repulsor-btn" id="cRep" onclick="cmd(3)" disabled><div class="dot"></div>РЕПУЛЬСОР (800 мс)</button>
      <button class="ctrl" id="cMgs" onclick="cmd(7)" disabled><div class="dot"></div>РУЧНАЯ ПОДАЧА</button>
      <button class="ctrl active-warn wide" id="cPzo" onclick="cmd(4)" disabled><div class="dot"></div>ПОДЖИГ (500 мс)</button>
      <button class="ctrl purge-btn wide" id="cPsh" onclick="cmd(5)" disabled><div class="dot"></div>ПРОДУТЬ (200 мс клапан)</button>
    </div>

    <div class="block warn">
      <div class="blk-lbl">Целевое давление (max 6.0 бар)</div>
      <div class="row" style="justify-content:center;">
        <button class="t-pm" onclick="stepTgt(-0.5)">−</button>
        <span class="t-val"><span id="tval">1.0</span><span class="t-unit"> бар</span></span>
        <button class="t-pm" onclick="stepTgt(+0.5)">+</button>
      </div>
      <div class="row"><input type="range" id="tslider" min="0.5" max="6.0" step="0.1" value="1.0" oninput="onSlide(this.value)"></div>
      <div class="qrow">
        <button class="qb active" onclick="setQ(1)">1</button><button class="qb" onclick="setQ(2)">2</button>
        <button class="qb" onclick="setQ(3)">3</button><button class="qb" onclick="setQ(4)">4</button>
        <button class="qb" onclick="setQ(5)">5</button><button class="qb" onclick="setQ(6)">6</button>
      </div>
      <button class="apply-btn" onclick="applyTgt()">УСТАНОВИТЬ ДАВЛЕНИЕ</button>
      <div class="ok-msg" id="tok"></div>
    </div>

    <div class="footer" id="foot"></div>
  </div>

  <!-- ════════ СТРАНИЦА: ГРАФИКИ ════════ -->
  <div class="page" id="page-graph">
    <div class="chart-box">
      <div class="chart-hdr">
        <span class="chart-lbl">Давление, бар</span>
        <div class="range-btns">
          <button class="range-btn active" onclick="setRange('1h',this)">1ч</button>
          <button class="range-btn" onclick="setRange('1d',this)">1д</button>
          <button class="range-btn" onclick="setRange('1w',this)">1н</button>
        </div>
      </div>
      <canvas id="chartP" height="120"></canvas>
    </div>
    <div class="chart-box">
      <div class="chart-hdr"><span class="chart-lbl">Температура, °C</span></div>
      <canvas id="chartT" height="90"></canvas>
    </div>
    <div class="data-note" id="histInfo">Точек в базе: 0</div>
  </div>

  <!-- ════════ СТРАНИЦА: НАСТРОЙКИ ════════ -->
  <div class="page" id="page-settings">
    <!-- ⚡ v6.0: электролизёр только через MOSFET — выбор драйвера убран -->
    <div class="block green">
      <div class="blk-lbl">Гистерезис электролизёра (MOSFET)</div>
      <div class="row">
        <label>Гистер.</label>
        <input type="range" id="hslider" min="0" max="1.0" step="0.05" value="0" oninput="onHyst(this.value)">
        <span class="val" id="hval">0.00 бар</span>
      </div>
      <div class="data-note">Вкл. при P &lt; цель − гистерезис, выкл. на цели · 0…1 бар</div>
    </div>

    <div class="block temp"><div class="blk-lbl">Вентилятор корпуса</div><div class="data-note">Постоянное питание, без реле и управления ESP32.</div></div>
    <div class="block temp" id="fanCard2"></div>

    <!-- Калибровка давления -->
    <div class="block green">
      <div class="blk-lbl">Калибровка датчика давления</div>
      <div class="data-note" style="margin-bottom:10px;">2 точки: при 0 бар запишите текущее напряжение (offset), затем масштаб (бар/В). Текущее давление: <b id="calCur">0.00</b> бар</div>
      <div class="row">
        <label>Offset В</label>
        <input type="range" id="calOffSlider" min="0" max="1.0" step="0.01" value="0.5" oninput="onCalOff(this.value)">
        <span class="val" id="calOffVal">0.50 В</span>
      </div>
      <div class="row">
        <label>Бар/В</label>
        <input type="range" id="calSclSlider" min="0.5" max="5.0" step="0.05" value="2.5" oninput="onCalScl(this.value)">
        <span class="val" id="calSclVal">2.50</span>
      </div>
      <button class="apply-btn" onclick="applyCal()">СОХРАНИТЬ КАЛИБРОВКУ</button>
      <div class="ok-msg" id="calOk"></div>
    </div>

    <a class="ota-link" id="otaLink" href="/update">⬆ OTA-обновление прошивки</a>
  </div>

  <!-- ════════ СТРАНИЦА: ДАННЫЕ/СТАТИСТИКА ════════ -->
  <div class="page" id="page-stats">
    <div class="stat-grid">
      <div class="stat-item"><div class="stat-k">Давление сейчас</div><div class="stat-v" id="sPnow">0.00</div></div>
      <div class="stat-item"><div class="stat-k">Давление макс</div><div class="stat-v" id="sPmax">0.00</div></div>
      <div class="stat-item"><div class="stat-k">Темп. сейчас</div><div class="stat-v" id="sTnow">0.0°</div></div>
      <div class="stat-item"><div class="stat-k">Темп. макс</div><div class="stat-v" id="sTmax">0.0°</div></div>
      <div class="stat-item"><div class="stat-k">Наработка</div><div class="stat-v sm" id="sRt">0ч 0м</div></div>
      <div class="stat-item"><div class="stat-k">Аптайм</div><div class="stat-v sm" id="sUp">0:00:00</div></div>
    </div>

    <div class="block warn">
      <div class="blk-lbl">Циклы реле <span style="font-size:.8em;color:var(--dim)">(ресурс ~100k)</span></div>
      <div class="stat-grid">
        <div class="stat-item"><div class="stat-k">Клапан</div><div class="stat-v sm" id="sRcv">0</div></div>
        <div class="stat-item"><div class="stat-k">Пьезо</div><div class="stat-v sm" id="sRcp">0</div></div>
        <div class="stat-item"><div class="stat-k">Выводной вентилятор</div><div class="stat-v sm" id="sRcf2">0</div></div>
        <div class="stat-item"><div class="stat-k">Электролизёр (MOSFET), переключений</div><div class="stat-v sm" id="sRce">0</div></div>
        <div class="stat-item"><div class="stat-k">RF-каналов</div><div class="stat-v sm" id="sRfh">к76 / 0</div></div>
      </div>
      <button class="apply-btn" style="margin-top:10px;border-color:var(--danger);color:var(--danger);background:rgba(255,51,85,.1);" onclick="resetCycles()">СБРОСИТЬ ЦИКЛЫ РЕЛЕ</button>
    </div>

    <div class="block green">
      <div class="blk-lbl">База данных истории (на телефоне)</div>
      <div class="data-btns">
        <button class="data-btn" onclick="exportCSV()">⬇ Экспорт CSV</button>
        <button class="data-btn" onclick="exportJSON()">⬇ Экспорт JSON</button>
        <button class="data-btn" onclick="document.getElementById('importFile').click()">⬆ Импорт файла</button>
        <button class="data-btn" style="border-color:var(--danger);color:var(--danger);" onclick="clearHistory()">🗑 Очистить базу</button>
      </div>
      <input type="file" id="importFile" accept=".json,.csv" style="display:none" onchange="importData(this)">
      <div class="data-note" id="dbInfo">Точек: 0 · Размер: 0 КБ · Авто-сохранение: вкл</div>
    </div>
    <button class="apply-btn" onclick="resetRT()" style="border-color:var(--danger);color:var(--danger);background:rgba(255,51,85,.1);">СБРОСИТЬ СЧЁТЧИК НАРАБОТКИ</button>
  </div>

  <!-- ════════ ⚡ v6.0 СТРАНИЦА: ЗВУК ════════ -->
  <div class="page" id="page-sound">
    <div class="block">
      <div class="blk-head"><span class="blk-lbl">Звук</span><span class="pill ok" id="sndPlay" style="visibility:hidden">ИГРАЕТ</span></div>
      <div class="sub-lbl">Где играть</div>
      <div class="seg" id="sndOutSeg">
        <button class="seg-opt" data-v="0" onclick="sndSetOut(0)">Пульт</button>
        <button class="seg-opt" data-v="1" onclick="sndSetOut(1)">Мозг</button>
        <button class="seg-opt" data-v="2" onclick="sndSetOut(2)">Оба</button>
        <button class="seg-opt" data-v="3" onclick="sndSetOut(3)">Выкл</button>
      </div>
      <div class="row">
        <label>Громкость</label>
        <input type="range" id="sndVol" min="1" max="10" step="1" value="10" oninput="sndOnVol(this.value)">
        <span class="val" id="sndVolVal">10</span>
      </div>
      <div class="opt-list">
        <label class="diag-row">Звуки тревог<input type="checkbox" class="sw" id="sndAlm" checked onchange="sndDirty=true"></label>
        <label class="diag-row">Щелчки кнопок<input type="checkbox" class="sw" id="sndClk" checked onchange="sndDirty=true"></label>
      </div>
      <div class="fld">
        <div class="sub-lbl">Мелодия при загрузке</div>
        <select class="inp" id="sndBoot" onchange="sndDirty=true"></select>
      </div>
      <div class="fld">
        <div class="sub-lbl">Мелодия при включении системы</div>
        <select class="inp" id="sndOn" onchange="sndDirty=true"></select>
      </div>
      <div class="btn-row">
        <button class="apply-btn" onclick="sndApply()">Применить</button>
        <button class="data-btn" onclick="sndTest()">Тест</button>
        <button class="data-btn" onclick="sndStop()">■ Стоп</button>
      </div>
      <div class="ok-msg" id="sndOk"></div>
    </div>

    <div class="block">
      <div class="blk-head"><span class="blk-lbl">Библиотека мелодий</span><span class="blk-aux" id="melSlots"></span></div>
      <div id="melList"><div class="mel-empty">Загрузка…</div></div>
      <div class="data-note" id="melInfo"></div>
      <div class="ok-msg" id="melOk"></div>
    </div>

    <details class="block fold" id="melAdd">
      <summary><span class="blk-lbl">Добавить мелодию</span></summary>
      <div class="file-pick">
        <button class="data-btn" onclick="document.getElementById('melFile').click()">Выбрать файл</button>
        <span class="file-name" id="melFileName">MIDI, RTTTL или аудио (mp3, wav, ogg, m4a)</span>
      </div>
      <input type="file" id="melFile" style="display:none" onchange="melOnFile(this)">
      <div class="fld">
        <div class="sub-lbl">или вставьте RTTTL-текст</div>
        <textarea class="inp" id="melText" rows="3" spellcheck="false" placeholder="Name:d=4,o=5,b=120:8c,8e,8g,c6" oninput="melOnText()"></textarea>
      </div>
      <div class="fld" id="melTrkFld" style="display:none">
        <div class="sub-lbl">Дорожка MIDI</div>
        <select class="inp" id="melTrk" onchange="melOnTrack()"></select>
      </div>
      <details class="fold sub">
        <summary>Параметры подготовки</summary>
        <div class="row lw"><label>Октавы</label>
          <select class="inp" id="melOct" onchange="melOnParam()">
            <option value="auto">Авто — под зуммер</option><option value="-3">−3</option><option value="-2">−2</option>
            <option value="-1">−1</option><option value="0">0</option><option value="1">+1</option>
            <option value="2">+2</option><option value="3">+3</option>
          </select></div>
        <div class="row lw"><label>Темп</label>
          <input type="range" id="melTempo" min="50" max="200" step="5" value="100" oninput="melOnParam()"><span class="val" id="melTempoV"></span></div>
        <div class="row lw"><label>Пауза нот</label>
          <input type="range" id="melGap" min="0" max="50" step="1" value="15" oninput="melOnParam()"><span class="val" id="melGapV"></span></div>
        <div class="row lw"><label>Макс. длина</label>
          <input type="range" id="melMax" min="5" max="120" step="5" value="30" oninput="melOnParam()"><span class="val" id="melMaxV"></span></div>
        <div class="row lw"><label>Анализ аудио</label>
          <input type="range" id="melSecs" min="5" max="120" step="5" value="30" oninput="melOnSecs()"><span class="val" id="melSecsV"></span></div>
        <div class="hint" style="margin:6px 0 0">Пауза нот разделяет соседние ноты, чтобы повторы не сливались в один тон. «Анализ аудио» — сколько первых секунд аудиофайла распознавать.</div>
      </details>
      <button class="apply-btn" id="melPrepBtn" onclick="melPrepareClick()">Подготовить</button>
      <div class="prog" id="melProg"><i id="melProgBar"></i></div>
      <div class="msg" id="melMsg"></div>
      <div class="mel-res" id="melRes">
        <div class="mel-stat" id="melStat"></div>
        <div class="hint" id="melStat2"></div>
        <div class="fld">
          <div class="sub-lbl">Название</div>
          <input class="inp" id="melName" maxlength="48" oninput="melNameAuto=false">
        </div>
        <div class="btn-row two">
          <button class="data-btn" id="melListenBtn" onclick="melListen()">Прослушать</button>
          <button class="apply-btn" id="melSaveBtn" onclick="melSave()">Сохранить в мозг</button>
        </div>
        <div class="msg" id="melSaveMsg"></div>
      </div>
      <div class="mel-tips">
        <p>Лучше всего подходят MIDI и RTTTL рингтонов. Аудио распознаётся по высоте тона — хорошо работает с простыми одноголосыми рингтонами, хуже — с песнями и аккордами.</p>
        <p>Мелодию из фильма «Бумер» (рингтон) загрузите сами: найдите файл .mid или RTTTL-текст рингтона (например, по запросу «бумер rtttl» / «бумер midi») или mp3 рингтона, выберите его здесь и нажмите «Подготовить» → «Сохранить в мозг», затем выберите её как мелодию загрузки.</p>
      </div>
    </details>
  </div>

  <div class="page" id="page-diag">
    <div class="block">
      <div class="blk-lbl">Состояние модулей (обновляется в реальном времени)</div>
      <div class="diag-row"><span>OLED-дисплей · I²C 0x3C</span><span class="pill" id="dgOled">…</span></div>
      <div class="diag-row"><span>MPU-6050 (наклон) · I²C 0x68</span><span class="pill" id="dgMpu">…</span></div>
      <div class="diag-row"><span>Радио NRF24L01 · SPI</span><span class="pill" id="dgNrf">…</span></div>
      <div class="diag-row"><span>Датчик температуры</span><span class="pill" id="dgTemp">…</span></div>
      <div class="diag-row"><span>Связь с пультом</span><span class="pill" id="dgRf">…</span></div>
      <div class="diag-row"><span>Выводной вентилятор (реле GPIO25)</span><span class="pill" id="dgFans">…</span></div>
      <div class="diag-row"><span>Память мелодий (LittleFS)</span><span class="pill" id="dgFs">…</span></div>
    </div>
    <div class="block">
      <div class="blk-lbl">Что проверить</div>
      <div class="diag-hint" id="dgHint">Загрузка состояния…</div>
    </div>
    <div class="block">
      <div class="blk-lbl">Подсказка по шине I²C</div>
      <div class="diag-hint">OLED и MPU-6050 сидят на одной шине I²C (пины 21/22). Если не работают оба сразу — причина общая: SDA→21, SCL→22 (на дисплее этот контакт подписан «SCK»), питание 3,3 В, общая земля, провода не задели TX/RX. Полная диагностика — в файле H2_Assembly_Full.html, раздел «ЭТАП E · Диагностика».</div>
    </div>
  </div>
</div>

<script>
/* ══════════ СОСТОЯНИЕ И WEBSOCKET ══════════ */
let st={}, pendingTgt=1.0, purging=false;
let ws=null, wsAlive=false, lastMsg=0;
let chartRange='1h';

/* История хранится в localStorage телефона. Структура: массив {ts,p,t}.
   Разрешение прореживаем по диапазону для экономии места. */
let history=[];
const HIST_KEY='h2pro_history';
const MAX_POINTS=20000;       // ~ неделя при записи раз в 30 сек
let lastHistPush=0;

function loadHistory(){
  try{ const s=localStorage.getItem(HIST_KEY); if(s) history=JSON.parse(s)||[]; }
  catch(e){ history=[]; }
}
function saveHistory(){
  try{ localStorage.setItem(HIST_KEY,JSON.stringify(history)); }catch(e){}
}
function pushHistory(p,t){
  const now=Date.now();
  if(now-lastHistPush < 30000) return;   // не чаще раза в 30 сек
  lastHistPush=now;
  history.push({ts:now,p:+p.toFixed(2),t:+t.toFixed(1)});
  if(history.length>MAX_POINTS) history=history.slice(-MAX_POINTS);
  saveHistory();
}

/* ══════════ ТЕМА ══════════ */
function toggleTheme(){
  document.body.classList.toggle('light');
  const light=document.body.classList.contains('light');
  document.getElementById('themeBtn').innerText=light?'☀':'🌙';
  localStorage.setItem('h2pro_theme',light?'light':'dark');
}
if(localStorage.getItem('h2pro_theme')==='light'){
  document.body.classList.add('light');
  document.getElementById('themeBtn').innerText='☀';
}

/* ══════════ ВКЛАДКИ ══════════ */
function showPage(p,btn){
  document.querySelectorAll('.page').forEach(e=>e.classList.remove('active'));
  document.querySelectorAll('.tab').forEach(e=>e.classList.remove('active'));
  document.getElementById('page-'+p).classList.add('active');
  btn.classList.add('active');
  if(p==='graph') drawCharts();
  if(p==='sound') sndLoadCfg();   // ⚡ v6.0: свежий список мелодий и настроек звука
}

/* ══════════ WEBSOCKET ══════════ */
function wsConnect(){
  ws=new WebSocket('ws://'+location.host+'/ws');
  ws.onopen=()=>{ wsAlive=true; document.getElementById('wsDot').classList.add('live'); };
  ws.onclose=()=>{ wsAlive=false; document.getElementById('wsDot').classList.remove('live'); setTimeout(wsConnect,2000); };
  ws.onerror=()=>{ ws.close(); };
  ws.onmessage=(ev)=>{ lastMsg=Date.now(); try{ st=JSON.parse(ev.data); render(); }catch(e){} };
}
function cmd(id){ if(wsAlive&&ws.readyState===1) ws.send('{"cmd":'+id+'}'); else fetch('/cmd?id='+id).catch(()=>{}); }

/* ══════════ ⚡ v6.0: МЕЛКИЕ ПОМОЩНИКИ ══════════ */
function byId(id){ return document.getElementById(id); }
function txt(id,v){ const e=byId(id); if(e) e.textContent=v; }
// Установить значение ползунка и закрасить дорожку (--pct) по его min/max
function setRng(e,v){ e.value=v; const mn=+e.min, mx=+e.max;
  e.style.setProperty('--pct',((+e.value-mn)/((mx-mn)||1)*100)+'%'); }
function esc(s){ return String(s).replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c])); }
function netErr(e){ return (e instanceof TypeError)?'нет связи с мозгом':((e&&e.message)||'ошибка'); }

/* ══════════ УПРАВЛЕНИЕ ДАВЛЕНИЕМ ══════════ */
function onSlide(v){ v=+v; pendingTgt=v; document.getElementById('tval').innerText=v.toFixed(1);
  document.getElementById('tslider').style.setProperty('--pct',((v-0.5)/5.5*100).toFixed(1)+'%'); updateQ(v); }
function setQ(v){ pendingTgt=v; document.getElementById('tslider').value=v; onSlide(v); }
function stepTgt(d){ let v=Math.max(0.5,Math.min(6.0,Math.round((pendingTgt+d)*2)/2)); setQ(v); }
function updateQ(v){ document.querySelectorAll('#page-main .qb').forEach((b,i)=>b.classList.toggle('active',(i+1)===v)); }
function applyTgt(){ fetch('/setpressure?val='+pendingTgt.toFixed(1)).catch(()=>{}); flash('tok','✓ установлено: '+pendingTgt.toFixed(1)+' бар'); }
function handlePower(){ if(purging)return;
  if(!window._pt){ window._pt=setTimeout(()=>{window._pt=null;cmd(1);},380); }
  else{ clearTimeout(window._pt); window._pt=null; cmd(99); } }

/* ══════════ ГИСТЕРЕЗИС (электролизёр — только MOSFET) ══════════ */
function onHyst(v){ const h=+v;
  document.getElementById('hval').innerText=h.toFixed(2)+' бар';
  document.getElementById('hslider').style.setProperty('--pct',(h*100)+'%');
  fetch('/sethysteresis?val='+h.toFixed(2)).catch(()=>{}); }

/* ══════════ ВЫВОДНОЙ ВЕНТИЛЯТОР НА РЕЛЕ ══════════
   Режимы: 0 Авто (по температуре), 1 Вкл, 2 Выкл, 3 При работе.
   Значения подтягиваются из статуса, пока пользователь не начал правку (dirty);
   после ПРИМЕНИТЬ флаг сбрасывается и карточка снова следует за мозгом. */
const FAN_MODES=['Авто','Вкл','Выкл','При работе'];
const FAN_HINT=['По температуре; без датчика и при перегреве — включён.',
  'Работает постоянно.',
  'Выключен всегда — даже при перегреве.',
  'Пока система включена или идёт продувка, иначе — по температуре.'];
const fanUi={2:{m:3,on:45,off:40,dirty:false,init:false}};
function fanCardHtml(i,title){
  return `<div class="blk-head"><span class="blk-lbl">${title}</span><span class="pill" id="fanSt${i}">—</span></div>
    <div class="seg">${FAN_MODES.map((n,m)=>`<button class="seg-opt" id="fan${i}m${m}"${m===3?' style="flex:1.45"':''} onclick="fanSetMode(${i},${m})">${n}</button>`).join('')}</div>
    <div class="hint" id="fanHint${i}"></div>
    <div id="fanThr${i}">
      <div class="row lw"><label>Включать при</label><input type="range" id="fanOn${i}" min="20" max="90" step="1" oninput="fanOnIn(${i},this.value)"><span class="val" id="fanOnV${i}"></span></div>
      <div class="row lw"><label>Выключать при</label><input type="range" id="fanOff${i}" min="15" max="89" step="1" oninput="fanOffIn(${i},this.value)"><span class="val" id="fanOffV${i}"></span></div>
    </div>
    <button class="apply-btn" onclick="fanApply(${i})">Применить</button>
    <div class="ok-msg" id="fanOk${i}"></div>`;
}
function fanPaint(i){
  const u=fanUi[i];
  for(let m=0;m<4;m++){ const b=byId('fan'+i+'m'+m); if(b) b.classList.toggle('active',m===u.m); }
  txt('fanHint'+i,FAN_HINT[u.m]||'');
  const thr=byId('fanThr'+i); if(thr) thr.style.display=(u.m===0||u.m===3)?'':'none';   // пороги нужны только Авто / При работе
  const on=byId('fanOn'+i), off=byId('fanOff'+i);
  if(on){ setRng(on,u.on); txt('fanOnV'+i,u.on+' °C'); }
  if(off){ off.max=u.on-1; setRng(off,u.off); txt('fanOffV'+i,u.off+' °C'); }
}
function fanSetMode(i,m){ fanUi[i].m=m; fanUi[i].dirty=true; fanPaint(i); }
function fanOnIn(i,v){ const u=fanUi[i]; u.on=+v; if(u.off>u.on-1) u.off=u.on-1; u.dirty=true; fanPaint(i); }
function fanOffIn(i,v){ const u=fanUi[i]; u.off=Math.min(+v,u.on-1); u.dirty=true; fanPaint(i); }
function fanApply(i){ const u=fanUi[i];
  fetch('/setfans?i='+i+'&mode='+u.m+'&ton='+u.on+'&toff='+u.off)
    .then(r=>{ if(!r.ok) throw new Error('ошибка '+r.status); u.dirty=false; flash('fanOk'+i,'✓ применено'); })
    .catch(e=>flash('fanOk'+i,'✗ '+netErr(e),true)); }
function fanSync(i,d){
  const k='f'+i, u=fanUi[i];
  if(u.dirty || d[k+'m']===undefined) return;
  const m=+d[k+'m'], on=Math.round(d[k+'on']), off=Math.round(d[k+'off']);
  if(u.init && u.m===m && u.on===on && u.off===off) return;
  u.m=m; u.on=on; u.off=off; u.init=true; fanPaint(i);
}
function fanState(i,on){
  const l=byId('fanL'+i); if(l) l.classList.toggle('on',on);
  txt('fanTx'+i,on?'ВКЛ':'ВЫКЛ');
  const p=byId('fanSt'+i); if(p){ p.className='pill'+(on?' ok':''); p.textContent=on?'ВКЛ':'ВЫКЛ'; }
}

/* ══════════ КАЛИБРОВКА ══════════ */
let calCfg={off:0.5,scl:2.5};
function onCalOff(v){ calCfg.off=+v; document.getElementById('calOffVal').innerText=(+v).toFixed(2)+' В';
  document.getElementById('calOffSlider').style.setProperty('--pct',(v/1.0*100)+'%'); }
function onCalScl(v){ calCfg.scl=+v; document.getElementById('calSclVal').innerText=(+v).toFixed(2);
  document.getElementById('calSclSlider').style.setProperty('--pct',((v-0.5)/4.5*100)+'%'); }
function applyCal(){ fetch('/calibrate?offset='+calCfg.off.toFixed(2)+'&scale='+calCfg.scl.toFixed(2)).catch(()=>{});
  flash('calOk','✓ калибровка сохранена'); }

/* ══════════ СБРОСЫ ══════════ */
function resetRT(){ if(confirm('Обнулить счётчик наработки?')) fetch('/resetruntime').catch(()=>{}); }
function resetCycles(){ if(confirm('Обнулить счётчики циклов реле?')) fetch('/resetcycles').catch(()=>{}); }

/* ══════════ ЭКСПОРТ/ИМПОРТ ══════════ */
function download(name,content,type){
  const blob=new Blob([content],{type:type});
  const a=document.createElement('a');
  a.href=URL.createObjectURL(blob); a.download=name;
  document.body.appendChild(a); a.click(); document.body.removeChild(a);
}
function exportCSV(){
  let csv='timestamp,iso,pressure_bar,temp_c\n';
  history.forEach(r=>{ csv+=r.ts+','+new Date(r.ts).toISOString()+','+r.p+','+r.t+'\n'; });
  download('h2pro_'+Date.now()+'.csv',csv,'text/csv');
}
function exportJSON(){ download('h2pro_'+Date.now()+'.json',JSON.stringify(history),'application/json'); }
function importData(inp){
  const f=inp.files[0]; if(!f)return;
  const r=new FileReader();
  r.onload=e=>{
    try{
      let data;
      if(f.name.endsWith('.json')){ data=JSON.parse(e.target.result); }
      else{ // CSV
        data=[]; const lines=e.target.result.split('\n');
        for(let i=1;i<lines.length;i++){ const c=lines[i].split(','); if(c.length>=4) data.push({ts:+c[0],p:+c[2],t:+c[3]}); }
      }
      if(Array.isArray(data)){
        // объединяем с текущей базой, сортируем по времени, убираем дубли
        const map={}; [...history,...data].forEach(r=>{ if(r&&r.ts) map[r.ts]=r; });
        history=Object.values(map).sort((a,b)=>a.ts-b.ts);
        if(history.length>MAX_POINTS) history=history.slice(-MAX_POINTS);
        saveHistory(); updateDbInfo(); drawCharts();
        alert('Импортировано. Всего точек: '+history.length);
      }
    }catch(err){ alert('Ошибка импорта: '+err); }
  };
  r.readAsText(f); inp.value='';
}
function clearHistory(){ if(confirm('Очистить всю базу истории на телефоне?')){ history=[]; saveHistory(); updateDbInfo(); drawCharts(); } }
function updateDbInfo(){
  const kb=(JSON.stringify(history).length/1024).toFixed(1);
  const el=document.getElementById('dbInfo'); if(el) el.innerText='Точек: '+history.length+' · Размер: '+kb+' КБ · Авто-сохранение: вкл';
  const hi=document.getElementById('histInfo'); if(hi) hi.innerText='Точек в базе: '+history.length;
}

/* ══════════ ГРАФИКИ ══════════ */
function setRange(r,btn){ chartRange=r;
  document.querySelectorAll('#page-graph .range-btn').forEach(b=>b.classList.remove('active'));
  btn.classList.add('active'); drawCharts(); }
function rangeMs(){ return chartRange==='1h'?3600e3:chartRange==='1d'?86400e3:604800e3; }
function drawLine(cv,data,key,color,maxY,unit){
  const ctx=cv.getContext('2d'); const w=cv.width=cv.clientWidth*2; const h=cv.height=cv.clientHeight*2;
  ctx.scale(1,1); ctx.clearRect(0,0,w,h);
  const css=getComputedStyle(document.body);
  ctx.strokeStyle=css.getPropertyValue('--border'); ctx.lineWidth=1;
  for(let i=0;i<=4;i++){ const y=i*(h-20)/4+10; ctx.beginPath(); ctx.moveTo(40,y); ctx.lineTo(w,y); ctx.stroke();
    ctx.fillStyle=css.getPropertyValue('--dim'); ctx.font='16px monospace'; ctx.textAlign='right';
    ctx.fillText((maxY*(1-i/4)).toFixed(1),36,y+5); }
  if(data.length<2){ ctx.fillStyle=css.getPropertyValue('--dim'); ctx.font='18px monospace'; ctx.textAlign='center';
    ctx.fillText('Накопление данных...',w/2,h/2); return; }
  const t0=data[0].ts, t1=data[data.length-1].ts, span=Math.max(1,t1-t0);
  ctx.strokeStyle=color; ctx.lineWidth=2.5; ctx.beginPath();
  data.forEach((r,i)=>{ const x=40+(r.ts-t0)/span*(w-40); const y=10+(1-Math.min(r[key]/maxY,1))*(h-20);
    i===0?ctx.moveTo(x,y):ctx.lineTo(x,y); });
  ctx.stroke();
}
function drawCharts(){
  const cut=Date.now()-rangeMs();
  const data=history.filter(r=>r.ts>=cut);
  drawLine(document.getElementById('chartP'),data,'p','#00d4ff',6.5,'бар');
  drawLine(document.getElementById('chartT'),data,'t','#ff8844',90,'°C');
  updateDbInfo();
}

/* ══════════ ⚡ v6.0: ЗВУК — НАСТРОЙКИ ══════════
   Вывод/громкость следуют за статусом (sout/svol), пока пользователь не начал
   правку (sndDirty). Остальное — из /snd/cfg при открытии вкладки. */
let sndCfg=null, sndDirty=false;
const sndUi={out:2,vol:10};
const SND_BUILTIN=[{id:-1,name:'H2 PRO',bi:1},{id:-2,name:'Сигнал включения',bi:1},{id:-3,name:'Тревога',bi:1}];
function sndSetOut(v){ sndUi.out=v; sndDirty=true; sndPaintOut(); }
function sndPaintOut(){ document.querySelectorAll('#sndOutSeg .seg-opt').forEach(b=>b.classList.toggle('active',+b.dataset.v===sndUi.out)); }
function sndOnVol(v){ sndUi.vol=+v; sndDirty=true; sndPaintVol(); }
function sndPaintVol(){ const s=byId('sndVol'); if(s) setRng(s,sndUi.vol); txt('sndVolVal',sndUi.vol); }
function sndLoadCfg(){
  return fetch('/snd/cfg').then(r=>{ if(!r.ok) throw new Error('ошибка '+r.status); return r.json(); })
    .then(c=>{ sndCfg=c;
      if(!sndDirty){ sndUi.out=c.out; sndUi.vol=c.vol; sndPaintOut(); sndPaintVol();
        const a=byId('sndAlm'), k=byId('sndClk'); if(a) a.checked=!!c.alm; if(k) k.checked=!!c.clk; }
      sndFillSel('sndBoot',c.boot,-1); sndFillSel('sndOn',c.on,-2); melRenderLib(); })
    .catch(()=>{ txt('melInfo','Нет ответа от мозга — список мелодий недоступен'); });
}
// Список «мелодия события»: Нет / встроенные / пользовательские. Во время правки выбор не сбрасываем.
function sndFillSel(id,val,def){
  const s=byId(id); if(!s) return;
  const want=(sndDirty&&s.value!=='')?s.value:String(val);
  s.innerHTML='';
  const add=(v,t)=>{ const o=document.createElement('option'); o.value=String(v); o.textContent=t; s.appendChild(o); };
  add(-9,'Нет'); add(-1,'H2 PRO (встроенная)'); add(-2,'Сигнал включения (встроенный)');
  ((sndCfg&&sndCfg.list)||[]).forEach(m=>add(m.id,m.name));
  s.value=want; if(s.value!==want) s.value=String(def);
}
function sndApply(){
  if(!sndCfg){ flash('sndOk','✗ нет данных от мозга — повторите',true); sndLoadCfg(); return; }
  const q='out='+sndUi.out+'&vol='+sndUi.vol+'&alm='+(byId('sndAlm').checked?1:0)+'&clk='+(byId('sndClk').checked?1:0)
    +'&boot='+byId('sndBoot').value+'&on='+byId('sndOn').value;
  fetch('/snd/set?'+q).then(r=>{ if(!r.ok) throw new Error('ошибка '+r.status);
      sndDirty=false; flash('sndOk','✓ настройки звука сохранены'); sndLoadCfg(); })
    .catch(e=>flash('sndOk','✗ '+netErr(e),true));
}
// Сыграть на устройстве — туда, что выбрано в «Где играть»
function sndPlayId(id,msgId){
  if(sndUi.out===3){ flash(msgId,'Звук выключен — выберите, где играть',true); return; }
  fetch('/snd/play?id='+id+'&out='+sndUi.out).then(r=>{
      if(r.status===404) throw new Error('мелодия не найдена'); if(!r.ok) throw new Error('ошибка '+r.status); })
    .catch(e=>flash(msgId,'✗ '+netErr(e),true));
}
function sndTest(){ sndPlayId(-1,'sndOk'); }
function sndStop(){ fetch('/snd/stop').catch(()=>{}); }

/* ══════════ ⚡ v6.0: ЗВУК — БИБЛИОТЕКА ══════════ */
const ICO={
  play:'<svg viewBox="0 0 24 24"><path d="M8 5.5v13l10.5-6.5z"/></svg>',
  ear:'<svg viewBox="0 0 24 24"><path d="M4 18v-5a8 8 0 0 1 16 0v5"/><path d="M4 15h3v6H5a1 1 0 0 1-1-1zM20 15h-3v6h2a1 1 0 0 0 1-1z"/></svg>',
  stop:'<svg viewBox="0 0 24 24"><rect x="6.5" y="6.5" width="11" height="11" rx="1.5"/></svg>',
  del:'<svg viewBox="0 0 24 24"><path d="M7 7l10 10M17 7L7 17"/></svg>'
};
function melFmtBytes(b){ return b>=1048576?(b/1048576).toFixed(2)+' МБ':Math.round(b/1024)+' КБ'; }
function melRowHtml(m){
  const meta=m.bi?'встроенная':(m.n+' нот · '+(m.ms/1000).toFixed(1)+' с');
  return `<div class="mel-row"><div class="mel-main"><div class="mel-name">${esc(m.name)}</div><div class="mel-meta">${meta}</div></div>`
    +`<button class="ico-btn" title="Сыграть на устройстве" onclick="melDevPlay(${m.id})">${ICO.play}</button>`
    +`<button class="ico-btn" data-pv="${m.id}" title="Прослушать в браузере" onclick="melPreview(${m.id})">${ICO.ear}</button>`
    +(m.bi?'<span class="ico-ph"></span>':`<button class="ico-btn del" title="Удалить" onclick="melDel(${m.id})">${ICO.del}</button>`)
    +'</div>';
}
function melRenderLib(){
  const box=byId('melList'); if(!box) return;
  const c=sndCfg, user=((c&&c.list)||[]).slice().sort((a,b)=>a.id-b.id);
  let h='';
  if(c&&!c.fsok) h+='<div class="mel-empty">Память мелодий (LittleFS) недоступна — работают только встроенные</div>';
  else if(c&&!user.length) h+='<div class="mel-empty">Своих мелодий пока нет — добавьте ниже</div>';
  h+=user.map(melRowHtml).join('')+SND_BUILTIN.map(melRowHtml).join('');
  box.innerHTML=h;
  txt('melSlots',c?user.length+' из 16 слотов':'');
  txt('melInfo',c?(c.fsok?'Свободно '+melFmtBytes(c.free)+' из '+melFmtBytes(c.total):''):'');
  melPlUi();
}
function melDevPlay(id){ sndPlayId(id,'melOk'); }
function melPreview(id){
  if(melPl&&melPl.owner==='lib'+id){ melStopPreview(); return; }
  try{ melCtx(); }catch(e){ flash('melOk','✗ '+e.message,true); return; }   // AudioContext — строго в обработчике нажатия (iOS)
  fetch('/snd/get?id='+id).then(r=>{ if(!r.ok) throw new Error('мелодия не найдена'); return r.arrayBuffer(); })
    .then(ab=>melPlay(melParseH2M(ab).notes,'lib'+id))
    .catch(e=>flash('melOk','✗ '+netErr(e),true));
}
function melDel(id){
  const m=((sndCfg&&sndCfg.list)||[]).find(x=>x.id===id);
  if(!confirm('Удалить мелодию «'+(m?m.name:id)+'»?')) return;
  if(melPl&&melPl.owner==='lib'+id) melStopPreview();
  fetch('/snd/del?id='+id).then(r=>{ if(!r.ok) throw new Error('ошибка '+r.status); flash('melOk','✓ удалено'); sndLoadCfg(); })
    .catch(e=>flash('melOk','✗ '+netErr(e),true));
}

/* ══════════ ⚡ v6.0: ПРОСЛУШИВАНИЕ В БРАУЗЕРЕ (WebAudio) ══════════
   Одна OscillatorNode 'square' + GainNode; вся мелодия планируется сразу. */
let melAc=null, melPl=null;
function melCtx(){
  if(!melAc){ const AC=window.AudioContext||window.webkitAudioContext;
    if(!AC) throw new Error('Браузер не поддерживает WebAudio'); melAc=new AC(); }
  if(melAc.state==='suspended') melAc.resume();
  return melAc;
}
function melPlay(notes,owner){
  melStopPreview();
  const ac=melCtx(), osc=ac.createOscillator(), g=ac.createGain();
  const V=0.1, R=0.004;   // громкость 0.1 и короткие фронты против щелчков
  osc.type='square'; g.gain.value=0; osc.connect(g); g.connect(ac.destination);
  const t0=ac.currentTime+0.05; let t=t0;
  for(const n of notes){ const d=n.d/1000;
    if(n.f>0){ osc.frequency.setValueAtTime(n.f,t);
      if(d>3*R){ g.gain.setValueAtTime(0,t); g.gain.linearRampToValueAtTime(V,t+R);
        g.gain.setValueAtTime(V,t+d-R); g.gain.linearRampToValueAtTime(0,t+d); }
      else{ g.gain.setValueAtTime(V,t); g.gain.setValueAtTime(0,t+d); } }
    t+=d; }
  osc.start(t0); osc.stop(t+0.02);
  melPl={osc,g,owner};
  osc.onended=()=>{ if(melPl&&melPl.osc===osc){ melPl=null; melPlUi(); } };
  melPlUi();
}
function melStopPreview(){
  if(!melPl) return;
  const p=melPl; melPl=null;
  try{ p.osc.onended=null; p.osc.stop(); }catch(e){}
  try{ p.osc.disconnect(); p.g.disconnect(); }catch(e){}
  melPlUi();
}
function melPlUi(){
  const own=melPl?melPl.owner:'';
  txt('melListenBtn',own==='res'?'■ Стоп':'Прослушать');
  document.querySelectorAll('[data-pv]').forEach(b=>{ const on=own==='lib'+b.dataset.pv;
    if(b.classList.contains('on')!==on||!b.innerHTML){ b.classList.toggle('on',on); b.innerHTML=on?ICO.stop:ICO.ear; } });
}

/* ══════════ ⚡ v6.0: ФОРМАТ H2M1 (общий с прошивкой мозга) ══════════
   'H2M1' | nameLen u8 (≤48) | name UTF-8 | noteCount u16 | {freq u16, dur u16}×N, little-endian */
function melNameBytes(s){
  const enc=new TextEncoder(), parts=[]; let n=0;
  for(const ch of s.replace(/[\x00-\x1f\x7f]/g,'')){ const b=enc.encode(ch); if(n+b.length>48) break; parts.push(b); n+=b.length; }
  const out=new Uint8Array(n); let p=0; parts.forEach(b=>{ out.set(b,p); p+=b.length; });
  return out;
}
function melBuildH2M(name,notes){
  const nb=melNameBytes(name), u=new Uint8Array(4+1+nb.length+2+notes.length*4), dv=new DataView(u.buffer);
  u.set([72,50,77,49],0); u[4]=nb.length; u.set(nb,5);
  let p=5+nb.length; dv.setUint16(p,notes.length,true); p+=2;
  for(const n of notes){ dv.setUint16(p,n.f,true); dv.setUint16(p+2,n.d,true); p+=4; }
  return u;
}
function melParseH2M(ab){
  const u=new Uint8Array(ab), dv=new DataView(ab);
  if(u.length<7||u[0]!==72||u[1]!==50||u[2]!==77||u[3]!==49) throw new Error('неверный формат мелодии');
  let p=5+u[4]; if(u.length<p+2) throw new Error('файл мелодии повреждён');
  const name=new TextDecoder().decode(u.subarray(5,p)), cnt=dv.getUint16(p,true); p+=2;
  if(u.length<p+cnt*4) throw new Error('файл мелодии повреждён');
  const notes=[]; for(let i=0;i<cnt;i++,p+=4) notes.push({f:dv.getUint16(p,true),d:dv.getUint16(p+2,true)});
  return {name,notes};
}

/* ══════════ ⚡ v6.0: РАЗБОР ИСТОЧНИКОВ ══════════
   Все разборщики дают «сырую» монофоническую мелодию [{m,d}]:
   m — номер ноты MIDI (A4=69=440 Гц), m<0 — пауза; d — длительность, мс. */
function melReadFile(f,asText){
  return new Promise((res,rej)=>{ const r=new FileReader();
    r.onload=()=>res(r.result); r.onerror=()=>rej(new Error('Не удалось прочитать файл'));
    if(asText) r.readAsText(f); else r.readAsArrayBuffer(f); });
}
function melText(u){
  let s=''; try{ s=new TextDecoder('utf-8',{fatal:true}).decode(u); }
  catch(e){ try{ s=new TextDecoder('windows-1251').decode(u); }catch(e2){ s=''; } }
  return s.replace(/[\x00-\x1f]/g,'').trim();
}

// RTTTL: «имя:d=4,o=5,b=63:8c,8e.,p,c6» — ноты [длит][нота][#][.][октава][.]
function melParseRtttl(src){
  const lines=String(src).split(/\r?\n/).map(s=>s.trim()).filter(Boolean);
  const i=lines.findIndex(l=>(l.match(/:/g)||[]).length>=2);
  if(i<0) throw new Error('Не похоже на RTTTL: нужен формат «имя:d=4,o=5,b=120:ноты»');
  let s=lines[i]; for(let j=i+1;j<lines.length&&lines[j].indexOf(':')<0;j++) s+=lines[j];   // ноты, перенесённые на след. строки
  const c1=s.indexOf(':'), c2=s.indexOf(':',c1+1), def={d:4,o:6,b:63};
  s.slice(c1+1,c2).split(',').forEach(kv=>{ const m=kv.trim().toLowerCase().match(/^([dob])\s*=\s*(\d+)$/); if(m) def[m[1]]=+m[2]; });
  if(!(def.b>0)||!(def.d>0)) throw new Error('RTTTL: неверные параметры d= или b=');
  const q=60000/def.b, SEMI={c:0,d:2,e:4,f:5,g:7,a:9,b:11,h:11}, raw=[];
  s.slice(c2+1).split(',').forEach(tk=>{ tk=tk.trim().toLowerCase(); if(!tk) return;
    const m=tk.match(/^(\d{1,2})?([a-hp])(#)?(\.)?(\d)?(\.)?$/);
    if(!m) throw new Error('RTTTL: не удалось разобрать «'+tk+'»');
    const len=m[1]?+m[1]:def.d; if(!(len>0)) throw new Error('RTTTL: нулевая длительность в «'+tk+'»');
    let d=4*q/len; if(m[4]||m[6]) d*=1.5;
    if(m[2]==='p') raw.push({m:-1,d});
    else raw.push({m:12*((m[5]!==undefined?+m[5]:def.o)+1)+SEMI[m[2]]+(m[3]?1:0),d});
  });
  if(!raw.some(x=>x.m>=0)) throw new Error('RTTTL: нет нот');
  return {name:s.slice(0,c1).trim(),raw};
}

// Карта темпа MIDI: тик → мс (по всем meta 0x51 всех дорожек)
function melTempoMap(tempos,div){
  tempos.sort((a,b)=>a.tick-b.tick);
  const seg=[{tick:0,ms:0,us:500000}];
  for(const tp of tempos){ const L=seg[seg.length-1];
    if(tp.tick===L.tick) L.us=tp.us;
    else seg.push({tick:tp.tick,ms:L.ms+(tp.tick-L.tick)*L.us/div/1000,us:tp.us}); }
  return tick=>{ let lo=0,hi=seg.length-1;
    while(lo<hi){ const mid=(lo+hi+1)>>1; if(seg[mid].tick<=tick) lo=mid; else hi=mid-1; }
    const g=seg[lo]; return g.ms+(tick-g.tick)*g.us/div/1000; };
}
// Standard MIDI File 0/1: дорожки с нотами (канал 10 — ударные — исключён)
function melParseMidi(ab){
  const b=new Uint8Array(ab);
  let base=-1;   // MThd может быть внутри RIFF/RMID — ищем в начале файла
  for(let i=0;i+4<=Math.min(b.length,64);i++) if(b[i]===77&&b[i+1]===84&&b[i+2]===104&&b[i+3]===100){ base=i; break; }
  if(base<0||b.length<base+14) throw new Error('Это не MIDI-файл (нет заголовка MThd)');
  const u16=p=>(b[p]<<8)|b[p+1], u32=p=>((b[p]<<24)>>>0)+(b[p+1]<<16)+(b[p+2]<<8)+b[p+3];
  const fmt=u16(base+8), ntr=u16(base+10), div=u16(base+12);
  if(div&0x8000) throw new Error('MIDI с SMPTE-временем не поддерживается');
  if(fmt>1) throw new Error('MIDI формата 2 не поддерживается (нужен формат 0 или 1)');
  if(!div) throw new Error('Повреждённый MIDI-файл');
  const tracks=[], tempos=[];
  let p=base+8+u32(base+4), tn=0;
  while(p+8<=b.length&&tn<ntr){
    const id=String.fromCharCode(b[p],b[p+1],b[p+2],b[p+3]), end=Math.min(p+8+u32(p+4),b.length);
    p+=8;
    if(id!=='MTrk'){ p=end; continue; }
    tn++;
    const tr={num:tn,name:'',notes:[]}, open=new Map();
    let tick=0, rs=0;
    const vlq=()=>{ let v=0,c; do{ if(p>=end) return v; c=b[p++]; v=v*128+(c&127); }while(c&128); return v; };
    while(p<end){
      tick+=vlq(); if(p>=end) break;
      let s=b[p];
      if(s&128) p++; else if(rs) s=rs; else break;   // running status
      if(s===0xFF){ const ty=b[p++], l=vlq(), q=p; p+=l;
        if(ty===0x51&&l>=3) tempos.push({tick,us:(b[q]<<16)|(b[q+1]<<8)|b[q+2]});
        else if(ty===0x03&&!tr.name) tr.name=melText(b.subarray(q,Math.min(q+l,end)));
        else if(ty===0x2F) break; }
      else if(s===0xF0||s===0xF7){ p+=vlq(); }
      else if(s>0xF0){ p+=(s===0xF2?2:s===0xF3?1:0); }
      else{ rs=s;
        const hi=s&0xF0, ch=s&15, one=(hi===0xC0||hi===0xD0), d1=b[p], d2=one?0:b[p+1];
        p+=one?1:2;
        if(ch===9) continue;   // ударные
        const key=ch*128+d1;
        if(hi===0x90&&d2>0){ if(!open.has(key)) open.set(key,[]); open.get(key).push(tick); }
        else if(hi===0x80||hi===0x90){ const qq=open.get(key);
          if(qq&&qq.length){ const s0=qq.shift(); if(tick>s0) tr.notes.push({s:s0,e:tick,n:d1}); } } }
    }
    open.forEach((qq,key)=>qq.forEach(s0=>{ if(tick>s0) tr.notes.push({s:s0,e:tick,n:key&127}); }));   // незакрытые
    p=end;
    if(tr.notes.length) tracks.push(tr);
  }
  if(!tracks.length) throw new Error('В MIDI-файле нет нот (ударные не считаются)');
  let auto=0, bn=-1, ba=-1;   // «Авто»: больше всего нот, при равенстве — выше средняя высота
  tracks.forEach((t,i)=>{ const avg=t.notes.reduce((a,x)=>a+x.n,0)/t.notes.length;
    if(t.notes.length>bn||(t.notes.length===bn&&avg>ba)){ bn=t.notes.length; ba=avg; auto=i; } });
  return {tracks,auto,toMs:melTempoMap(tempos,div)};
}
// Полифония → монофония «по верхнему голосу» (skyline): в каждый момент начала нот
// берём самую высокую; предыдущая нота обрывается началом следующей.
function melMidiRaw(mid,sel){
  const src=sel==='all'?[].concat(...mid.tracks.map(t=>t.notes)):mid.tracks[sel==='auto'?mid.auto:+sel].notes;
  const a=src.slice().sort((x,y)=>x.s-y.s||y.n-x.n), top=[];
  for(const x of a){ const L=top[top.length-1]; if(!L||L.s!==x.s) top.push(x); }
  const raw=[]; let t=0;
  for(let i=0;i<top.length;i++){ const x=top[i], s=mid.toMs(x.s);
    let e=mid.toMs(x.e); if(i+1<top.length) e=Math.min(e,mid.toMs(top[i+1].s));
    if(s>t) raw.push({m:-1,d:s-t});
    if(e>s){ raw.push({m:x.n,d:e-s}); t=e; } else t=Math.max(t,s); }
  return raw;
}

// Аудио → мелодия: моно, ~11 кГц, YIN (окно 512, шаг 12 мс, 70–2000 Гц), порог тишины −35 дБ,
// полутона, медиана по 5 кадрам, склейка, отбрасывание < 50 мс. Считается кусками с отдачей управления.
async function melAudioRaw(file,secs){
  melProg(0,'Декодирование аудио…');
  const ab=await melReadFile(file,false), ac=melCtx();
  let buf;
  try{ buf=await new Promise((res,rej)=>{ const pr=ac.decodeAudioData(ab,res,rej); if(pr&&pr.then) pr.then(res,rej); }); }
  catch(e){ throw new Error('Не удалось декодировать аудио — этот формат не поддерживается браузером'); }
  const sr0=buf.sampleRate, fac=Math.max(1,Math.round(sr0/11025)), sr=sr0/fac, chs=buf.numberOfChannels;
  const n=Math.floor(Math.min(buf.length,Math.floor(secs*sr0))/fac), ch=[];
  for(let c=0;c<chs;c++) ch.push(buf.getChannelData(c));
  const x=new Float32Array(n), kk=1/(fac*chs);
  for(let i=0;i<n;i++){ let s=0; const o=i*fac;   // понижение частоты усреднением + сведение в моно
    for(let c=0;c<chs;c++){ const a=ch[c]; for(let j=0;j<fac;j++) s+=a[o+j]; } x[i]=s*kk; }
  const W=512, tMin=Math.max(2,Math.floor(sr/2000)), tMax=Math.min(Math.ceil(sr/70),W>>1), L=W-tMax;
  const hop=Math.max(1,Math.round(sr*0.012)), nf=n>=W?Math.floor((n-W)/hop)+1:0;
  if(nf<5) throw new Error('Аудиофрагмент слишком короткий');
  const f0=new Float32Array(nf), rms=new Float32Array(nf), d=new Float32Array(tMax+1);
  let tY=performance.now();
  for(let fi=0;fi<nf;fi++){
    const o=fi*hop; let e=0;
    for(let j=0;j<W;j++){ const v=x[o+j]; e+=v*v; }
    rms[fi]=Math.sqrt(e/W);
    if(e>0){
      let run=0;   // разностная функция + кумулятивная нормировка (CMNDF)
      for(let t=1;t<=tMax;t++){ let s=0; for(let j=0;j<L;j++){ const v=x[o+j]-x[o+j+t]; s+=v*v; }
        run+=s; d[t]=run>0?s*t/run:1; }
      let bt=-1;
      for(let t=tMin;t<=tMax;t++) if(d[t]<0.15){ while(t<tMax&&d[t+1]<d[t]) t++; bt=t; break; }
      if(bt>0){ let tau=bt;
        if(bt>1&&bt<tMax){ const a=d[bt-1], c=d[bt+1], den=a-2*d[bt]+c; if(den>1e-9) tau=bt+0.5*(a-c)/den; }   // парабола
        const f=sr/tau; if(f>=70&&f<=2000) f0[fi]=f; }
    }
    if(performance.now()-tY>30){ melProg(fi/nf,'Анализ аудио…'); await new Promise(r=>setTimeout(r,0)); tY=performance.now(); }
  }
  let mx=0; for(let i=0;i<nf;i++) if(rms[i]>mx) mx=rms[i];
  const thr=mx*Math.pow(10,-35/20), mi=new Int16Array(nf), md=new Int16Array(nf), w=[0,0,0,0,0];
  for(let i=0;i<nf;i++) mi[i]=(f0[i]>0&&mx>0&&rms[i]>=thr)?Math.round(69+12*Math.log2(f0[i]/440)):0;
  for(let i=0;i<nf;i++){ for(let k=-2;k<=2;k++) w[k+2]=mi[Math.min(nf-1,Math.max(0,i+k))];
    md[i]=w.slice().sort((a,b)=>a-b)[2]; }
  const fms=hop/sr*1000, out=[];
  for(let i=0;i<nf;){ let j=i; while(j<nf&&md[j]===md[i]) j++;
    const sg={m:md[i]>0?md[i]:-1,d:(j-i)*fms}, Lp=out[out.length-1];
    if(Lp&&(sg.d<50||Lp.m===sg.m)) Lp.d+=sg.d; else out.push(sg);
    i=j; }
  if(!out.some(s=>s.m>=0)) throw new Error('В аудио не найдено устойчивых нот — лучше всего подходят простые одноголосые рингтоны');
  return out;
}

/* ══════════ ⚡ v6.0: ПОДГОТОВКА ПОД ЗУММЕР ══════════ */
function melHz(m){ return 440*Math.pow(2,(m-69)/12); }
function melPrepare(raw,o){
  if(!raw.some(x=>x.m>=0&&x.d>0)) throw new Error('В источнике не найдено ни одной ноты');
  let k=0;
  if(o.oct==='auto'){ let best=-1;   // сдвиг, при котором больше всего звучания в 700–3500 Гц
    for(const kk of [0,1,-1,2,-2,3,-3,4,-4]){ let s=0;
      for(const x of raw) if(x.m>=0){ const f=melHz(x.m+12*kk); if(f>=700&&f<=3500) s+=x.d; }
      if(s>best){ best=s; k=kk; } } }
  else k=o.oct;
  const sc=100/o.tempo;
  // 1) сдвиг октав, сворачивание в 200–5000 Гц, темп; соседние паузы — вместе
  const a=[];
  for(const x of raw){ if(!(x.d>0)) continue;
    let m=-1; if(x.m>=0){ m=x.m+12*k; while(melHz(m)<200) m+=12; while(melHz(m)>5000) m-=12; }
    const Lp=a[a.length-1]; if(m<0&&Lp&&Lp.m<0) Lp.d+=x.d*sc; else a.push({m,d:x.d*sc}); }
  // 2) ноты короче 25 мс и паузы короче 10 мс — в предыдущий элемент
  const b=[];
  for(const x of a){ const Lp=b[b.length-1];
    if(Lp&&((x.m>=0&&x.d<25)||(x.m<0&&(x.d<10||Lp.m<0)))) Lp.d+=x.d; else b.push(x); }
  // 3) артикуляция: между соседними нотами без паузы — короткая пауза
  const c=[];
  for(let i=0;i<b.length;i++){ const x=b[i], nx=b[i+1];
    if(o.gap>0&&x.m>=0&&nx&&nx.m>=0){ const g=Math.min(o.gap,x.d*0.25); c.push({m:x.m,d:x.d-g},{m:-1,d:g}); }
    else c.push(x); }
  // 4) паузы < 10 мс убрать (время — предыдущей ноте), соседние паузы объединить
  const e=[];
  for(const x of c){ const Lp=e[e.length-1];
    if(x.m<0&&Lp&&Lp.m<0){ Lp.d+=x.d; continue; }
    if(x.m<0&&x.d<10){ if(Lp) Lp.d+=x.d; continue; }
    e.push(x); }
  // 5) паузы по краям — прочь; 6) звуки длиннее 10 с — на части
  while(e.length&&e[0].m<0) e.shift();
  while(e.length&&e[e.length-1].m<0) e.pop();
  const f=[];
  for(const x of e){ const parts=Math.ceil(x.d/10000); for(let i=0;i<parts;i++) f.push({m:x.m,d:x.d/parts}); }
  // 7) ограничения: ≤ 1200 нот и ≤ макс. длительности
  const g=[]; let tot=0, cutT=false, cutN=false;
  for(const x of f){
    if(g.length>=1200){ cutN=true; break; }
    if(tot+x.d>o.maxMs){ const rest=o.maxMs-tot; if(x.m>=0&&rest>=25) g.push({m:x.m,d:rest}); cutT=true; break; }
    g.push(x); tot+=x.d; }
  while(g.length&&g[g.length-1].m<0) g.pop();
  // 8) целые мс (накопленное округление — без дрейфа темпа), частоты — равномерная темперация
  const notes=[]; let acc=0, prev=0, fmin=1e9, fmax=0;
  for(const x of g){ acc+=x.d; let d=Math.round(acc)-prev; d=Math.max(1,Math.min(10000,d)); prev+=d;
    const fr=x.m<0?0:Math.round(melHz(x.m));
    if(fr){ fmin=Math.min(fmin,fr); fmax=Math.max(fmax,fr); }
    notes.push({f:fr,d}); }
  if(!fmax) throw new Error('После подготовки не осталось нот');
  return {notes,k,auto:o.oct==='auto',ms:prev,fmin,fmax,cutT,cutN,maxMs:o.maxMs};
}

/* ══════════ ⚡ v6.0: ИМПОРТ — ИНТЕРФЕЙС ══════════
   Поток: файл / RTTTL-текст → «Подготовить» → «Прослушать» → «Сохранить в мозг».
   После первой подготовки смена параметров пересчитывает результат сразу. */
const MEL_FILE_HINT='MIDI, RTTTL или аудио (mp3, wav, ogg, m4a)';
let melSrc=null, melMidi=null, melAudio=null, melLastRaw=null, melRes=null, melBusy=false, melNameAuto=true;
function melMsgSet(id,t,c){ const e=byId(id); if(!e) return; e.textContent=t; e.className='msg'+(c?' '+c:''); }
function melProg(fr,label){
  const p=byId('melProg'); if(!p) return;
  if(fr<0){ p.classList.remove('show'); return; }
  p.classList.add('show'); byId('melProgBar').style.width=Math.round(fr*100)+'%';
  melMsgSet('melMsg',label+(fr>0?' '+Math.round(fr*100)+' %':''),'');
}
function melSetName(n){ if(melNameAuto){ const e=byId('melName'); if(e) e.value=n||'Мелодия'; } }
function melOpts(){ const o=byId('melOct').value;
  return {oct:o==='auto'?'auto':+o,tempo:+byId('melTempo').value,gap:+byId('melGap').value,maxMs:+byId('melMax').value*1000}; }
function melClearRes(){
  if(melPl&&melPl.owner==='res') melStopPreview();
  melRes=null; const r=byId('melRes'); if(r) r.classList.remove('show'); melMsgSet('melSaveMsg','','');
}
function melNewSource(){ melClearRes(); melLastRaw=null; melMidi=null; melNameAuto=true;
  byId('melTrkFld').style.display='none'; melMsgSet('melMsg','',''); }
async function melOnFile(inp){
  const f=inp.files[0]; inp.value=''; if(!f) return;
  melNewSource(); melSrc=null; byId('melText').value='';
  const ext=(f.name.split('.').pop()||'').toLowerCase();
  const kind=['mid','midi','kar','rmi'].includes(ext)?'midi'
    :['rtttl','rtx','txt'].includes(ext)?'rtttl'
    :(['mp3','wav','ogg','oga','opus','m4a','aac','flac','webm'].includes(ext)||/^audio\//.test(f.type))?'audio':null;
  txt('melFileName',f.name);
  if(!kind){ txt('melFileName',MEL_FILE_HINT); melMsgSet('melMsg','Формат не поддерживается: нужен .mid, .rtttl/.txt или аудиофайл','bad'); return; }
  melSrc={kind,file:f}; melSetName(f.name.replace(/\.[^.]*$/,''));
  if(kind==='midi'){
    try{ melMidi=melParseMidi(await melReadFile(f,false)); }
    catch(e){ melSrc=null; melMsgSet('melMsg',e.message,'bad'); return; }
    const s=byId('melTrk'); s.innerHTML='';
    const add=(v,t)=>{ const o=document.createElement('option'); o.value=String(v); o.textContent=t; s.appendChild(o); };
    const lbl=t=>t.num+': '+(t.name||'Дорожка')+' ('+t.notes.length+' нот)';
    add('auto','Авто — '+lbl(melMidi.tracks[melMidi.auto])); add('all','Все дорожки (верхний голос)');
    melMidi.tracks.forEach((t,i)=>add(i,lbl(t)));
    byId('melTrkFld').style.display='';
  }
  else if(kind==='audio') melMsgSet('melMsg','Аудио распознаётся при подготовке — это может занять несколько секунд.','');
}
function melOnText(){
  const has=byId('melText').value.trim()!=='';
  if(!melSrc||melSrc.kind!=='text'){ melNewSource(); txt('melFileName',MEL_FILE_HINT); }
  else melClearRes();
  melSrc=has?{kind:'text'}:null;
}
async function melGetRaw(){
  if(!melSrc) throw new Error('Выберите файл или вставьте RTTTL-текст');
  if(melSrc.kind==='midi') return melMidiRaw(melMidi,byId('melTrk').value);
  if(melSrc.kind==='text'||melSrc.kind==='rtttl'){
    const isText=melSrc.kind==='text';
    const r=melParseRtttl(isText?byId('melText').value:await melReadFile(melSrc.file,true));
    if(r.name||isText) melSetName(r.name);   // у файла без имени в RTTTL остаётся имя файла
    return r.raw;
  }
  const secs=+byId('melSecs').value;   // аудио: результат анализа кэшируется
  if(!(melAudio&&melAudio.file===melSrc.file&&melAudio.secs===secs))
    melAudio={file:melSrc.file,secs,raw:await melAudioRaw(melSrc.file,secs)};
  return melAudio.raw;
}
function melRunPrep(raw){
  if(melPl&&melPl.owner==='res') melStopPreview();
  melRes=melPrepare(raw,melOpts()); melLastRaw=raw;
  const r=melRes, ex=['сдвиг октав '+(r.k>0?'+':'')+String(r.k).replace('-','−')+(r.auto?' (авто)':'')];
  if(r.cutT) ex.push('обрезано до '+(r.maxMs/1000)+' с');
  if(r.cutN) ex.push('обрезано до 1200 нот');
  txt('melStat',r.notes.length+' нот · '+(r.ms/1000).toFixed(1)+' с · '+r.fmin+'–'+r.fmax+' Гц');
  txt('melStat2',ex.join(' · '));
  byId('melRes').classList.add('show'); melMsgSet('melSaveMsg','','');
}
async function melPrepareClick(){
  if(melBusy) return;
  const btn=byId('melPrepBtn'); melBusy=true; btn.disabled=true; melMsgSet('melMsg','','');
  try{ if(melSrc&&melSrc.kind==='audio') melCtx();   // AudioContext — в обработчике нажатия (iOS)
    melRunPrep(await melGetRaw()); melMsgSet('melMsg','Готово — прослушайте и сохраните.','ok'); }
  catch(e){ melClearRes(); melMsgSet('melMsg',(e&&e.message)||String(e),'bad'); }
  finally{ melBusy=false; btn.disabled=false; melProg(-1); }
}
function melReprep(raw){
  try{ melRunPrep(raw); }catch(e){ melClearRes(); melMsgSet('melMsg',e.message,'bad'); }
}
function melParamLabels(){
  const t=byId('melTempo'), g=byId('melGap'), m=byId('melMax'), s=byId('melSecs');
  setRng(t,t.value); setRng(g,g.value); setRng(m,m.value); setRng(s,s.value);
  txt('melTempoV',t.value+' %'); txt('melGapV',g.value+' мс'); txt('melMaxV',m.value+' с'); txt('melSecsV',s.value+' с');
}
function melOnParam(){
  melParamLabels();
  if(melRes&&melLastRaw&&!melBusy) melReprep(melLastRaw);
}
function melOnSecs(){
  melParamLabels();
  if(melSrc&&melSrc.kind==='audio'&&melRes&&!melBusy){ melClearRes(); melLastRaw=null;
    melMsgSet('melMsg','Длительность анализа изменена — нажмите «Подготовить».',''); }
}
function melOnTrack(){ if(melMidi&&melRes&&!melBusy) melReprep(melMidiRaw(melMidi,byId('melTrk').value)); }
function melListen(){
  if(melPl&&melPl.owner==='res'){ melStopPreview(); return; }
  if(!melRes) return;
  try{ melPlay(melRes.notes,'res'); }catch(e){ melMsgSet('melSaveMsg',e.message,'bad'); }
}
async function melSave(){
  if(!melRes) return;
  if(sndCfg&&!sndCfg.fsok){ melMsgSet('melSaveMsg','✗ Память мелодий (LittleFS) недоступна','bad'); return; }
  if(sndCfg&&(sndCfg.list||[]).length>=16){ melMsgSet('melSaveMsg','✗ Все 16 слотов заняты — удалите ненужную мелодию','bad'); return; }
  const fd=new FormData();
  fd.append('f',new Blob([melBuildH2M(byId('melName').value.trim()||'Мелодия',melRes.notes)],{type:'application/octet-stream'}),'melody.h2m');
  const btn=byId('melSaveBtn'); btn.disabled=true; melMsgSet('melSaveMsg','Загрузка…','');
  try{
    const r=await fetch('/snd/up',{method:'POST',body:fd}), t=await r.text();
    let j=null; try{ j=JSON.parse(t); }catch(e){}
    if(j&&j.ok){ melMsgSet('melSaveMsg','✓ Сохранено (слот '+j.id+'). Её можно выбрать мелодией события выше.','ok'); await sndLoadCfg(); }
    else melMsgSet('melSaveMsg','✗ '+((j&&j.err)||t||('ошибка '+r.status)),'bad');
  }catch(e){ melMsgSet('melSaveMsg','✗ '+netErr(e),'bad'); }
  finally{ btn.disabled=false; }
}

/* ══════════ РЕНДЕР ══════════ */
function cls(id,v,c){ const e=document.getElementById(id); if(e) e.className='badge'+(v?' '+c:''); }
function setPill(id,ok,a,b){ const e=document.getElementById(id); if(!e)return; e.className='pill '+(ok?'ok':'bad'); e.innerText=ok?a:b; }
function flash(id,msg,bad){ const e=document.getElementById(id); if(!e)return; e.innerText=msg;
  e.style.color=bad?'var(--danger)':''; e.style.opacity='1';
  clearTimeout(e._ft); e._ft=setTimeout(()=>e.style.opacity='0',bad?4000:2500); }
function fmtUp(s){ return [Math.floor(s/3600),Math.floor(s%3600/60),s%60].map(n=>String(n).padStart(2,'0')).join(':'); }

function render(){
  const d=st; if(d.p===undefined)return;
  const p=d.p;
  document.getElementById('pv').innerText=p.toFixed(2);
  document.getElementById('pf').style.width=Math.min(p/6.5*100,100)+'%';
  document.getElementById('ptgt').style.left='calc('+Math.min(d.tgt/6.5*100,100)+'% - 1px)';
  const ph=document.getElementById('physt');
  if(d.h>0){ const l=Math.max(0,(d.tgt-d.h)/6.5*100); ph.style.left=l+'%'; ph.style.width=Math.min(d.h/6.5*100,100-l)+'%'; }
  else ph.style.width='0%';

  const pBox=document.getElementById('pBox'),pNum=document.getElementById('pNum'),pf=document.getElementById('pf');
  pBox.className='pressure';pNum.className='p-num';pf.className='p-fill';
  if(d.ovl){pBox.classList.add('danger');pNum.classList.add('danger');pf.classList.add('danger');}
  else if(d.ovw){pBox.classList.add('warn');pNum.classList.add('warn');pf.classList.add('warn');}

  // Температура и выводной вентилятор на реле.
  const tv=document.getElementById('tempVal');
  tv.innerText=(d.tok?d.t.toFixed(1):'--')+'°';
  tv.className='temp-val'+(d.oh?' hot':'');
  fanState(2,!!d.f2);
  fanSync(2,d);

  // Tilt
  const tRow=document.getElementById('tiltRow'),tVal=document.getElementById('tiltVal'),tLbl=document.getElementById('tiltLbl');
  tRow.className='tilt-row';tVal.className='tilt-val';
  if(d.tcal){tLbl.innerText='КАЛИБРОВКА MPU6050...';tVal.innerText='wait';}
  else if(d.tlk){tRow.classList.add('danger');tVal.classList.add('danger');tLbl.innerText='НАКЛОН — АВАРИЯ (LOCK)';tVal.innerText=d.ta+'°';}
  else if(d.ta>=20){tRow.classList.add('warn');tVal.classList.add('warn');tLbl.innerText='НАКЛОН (порог 30°)';tVal.innerText=d.ta+'°';}
  else {tLbl.innerText='НАКЛОН (порог 30°)';tVal.innerText=d.ta+'°';}

  // Алерты (приоритет)
  const ab=document.getElementById('alertBar');
  if(d.tlk){ab.className='alert-bar show tilt';ab.innerText='⚠ НАКЛОН '+d.ta+'° — ЭЛЕКТРОЛИЗЕР И КЛАПАН ОБЕСТОЧЕНЫ';}
  else if(d.ovl){ab.className='alert-bar show danger';ab.innerText='⚠ АВАРИЯ '+p.toFixed(2)+' БАР';}
  else if(d.oh){ab.className='alert-bar show danger';ab.innerText='🔥 ПЕРЕГРЕВ '+d.t.toFixed(0)+'°C';}
  else if(d.ovw){ab.className='alert-bar show';ab.innerText='⚠ Приближение к аварии: '+p.toFixed(2)+' / 6.5 бар';}
  else if(d.wd){ab.className='alert-bar show';ab.innerText='⚠ БАК НЕ НАБИРАЕТ ДАВЛЕНИЕ';}
  else if(d.relw){ab.className='alert-bar show';ab.innerText='⚙ РЕСУРС РЕЛЕ НА ИСХОДЕ — проверьте контакты';}
  else ab.className='alert-bar';

  // Бейджи
  cls('bPwr',d.pwr,'ok');cls('bLck',d.lck,'warn');cls('bTlt',d.tlk,'tilt');
  cls('bPrg',d.prg,'danger');cls('bPsh',d.psh,'warn');cls('bRep',d.rep,'purple');
  cls('bMgs',d.mgs,'ok');cls('bPzo',d.pzo,'warn');cls('bEle',d.ele,'ok');cls('bVal',d.val,'ok');

  // Power btn + controls
  const bp=document.getElementById('btnPwr');
  bp.className='btn-power'+(d.pwr?' on':'');
  bp.innerText=d.pwr?'ВЫКЛЮЧИТЬ  (2× = ПРОДУВКА)':'ВКЛЮЧИТЬ СИСТЕМУ';
  const dis=!d.pwr||d.prg||d.lck||d.tlk;
  ['cRep','cMgs','cPzo','cPsh'].forEach(i=>document.getElementById(i).disabled=dis);
  document.getElementById('cLck').className='ctrl wide'+(d.lck?' active-warn':'');
  document.getElementById('cRep').className='ctrl repulsor-btn'+(d.rep?' active-purple':'');
  document.getElementById('cMgs').className='ctrl'+(d.mgs?' active':'');
  document.getElementById('cPzo').className='ctrl active-warn wide'+(d.pzo?' active-red':'');
  document.getElementById('cPsh').className='ctrl purge-btn wide'+(d.psh?' active-red':'');

  // Гистерезис (если не редактируем)
  const hs=document.getElementById('hslider');
  if(document.activeElement!==hs){ hs.value=d.h; document.getElementById('hval').innerText=d.h.toFixed(2)+' бар'; hs.style.setProperty('--pct',(d.h*100)+'%'); }

  // Целевое (если не редактируем)
  const sl=document.getElementById('tslider');
  if(document.activeElement!==sl){ const v=+d.tgt; sl.value=v; document.getElementById('tval').innerText=v.toFixed(1);
    sl.style.setProperty('--pct',((v-0.5)/5.5*100).toFixed(1)+'%'); pendingTgt=v; updateQ(Math.round(v)); }

  // RF / uptime / footer
  document.getElementById('rfDot').className='rf-dot'+(d.rf?' ok':' fail');
  document.getElementById('rfTxt').innerText='ПУЛЬТ: '+(d.rf?'ОНЛАЙН':'ОФФЛАЙН');
  document.getElementById('uptime').innerText='UP '+fmtUp(d.up);

  const ota=document.getElementById('otaLink');
  if(d.pwr){ ota.className='ota-link disabled'; ota.removeAttribute('href'); ota.innerText='⬆ OTA — выключите систему'; }
  else{ ota.className='ota-link'; ota.setAttribute('href','/update'); ota.innerText='⬆ OTA-обновление прошивки'; }

  purging=d.prg;
  const foot=document.getElementById('foot');
  if(d.prg)foot.innerText='⏳ ПРОДУВКА ДО НУЛЯ — '+p.toFixed(2)+' бар';
  else if(d.psh)foot.innerText='⚡ МИНИ-ПРОДУВКА 200 мс';
  else if(d.rep)foot.innerText='🔥 РЕПУЛЬСОР — ВЫСТРЕЛ';
  else if(d.tlk)foot.innerText='⚠ TILT — выровняйте и нажмите LOCK';
  else if(d.ovl)foot.innerText='⚠ АВАРИЯ — СБРОСЬТЕ ДАВЛЕНИЕ';
  else foot.innerText='';

  // Статистика
  document.getElementById('sPnow').innerText=p.toFixed(2);
  document.getElementById('sTnow').innerText=(d.tok?d.t.toFixed(1):'--')+'°';
  document.getElementById('sRt').innerText=Math.floor(d.rt/3600)+'ч '+Math.floor((d.rt%3600)/60)+'м';
  document.getElementById('sUp').innerText=fmtUp(d.up);
  document.getElementById('sRce').innerText=(d.rce||0).toLocaleString();
  document.getElementById('sRcv').innerText=(d.rcv||0).toLocaleString();
  document.getElementById('sRcp').innerText=(d.rcp||0).toLocaleString();
  txt('sRcf2',(d.rcf2||0).toLocaleString());
  document.getElementById('sRfh').innerText='к'+(d.rfc||76)+' / '+(d.rfh||0);
  document.getElementById('calCur').innerText=p.toFixed(2);
  // макс из истории
  if(history.length){ let pm=0,tm=0; history.forEach(r=>{if(r.p>pm)pm=r.p;if(r.t>tm)tm=r.t;});
    document.getElementById('sPmax').innerText=Math.max(pm,p).toFixed(2);
    document.getElementById('sTmax').innerText=Math.max(tm,d.tok?d.t:0).toFixed(1)+'°'; }
  else { document.getElementById('sPmax').innerText=p.toFixed(2);
    document.getElementById('sTmax').innerText=(d.tok?d.t.toFixed(1):'--')+'°'; }

  // ⚡ v6.0: звук — вывод/громкость (пока не редактируем) и индикатор «играет»
  if(d.sout!==undefined&&!sndDirty&&(sndUi.out!==d.sout||sndUi.vol!==d.svol)){
    sndUi.out=d.sout; sndUi.vol=d.svol||10; sndPaintOut(); sndPaintVol(); }
  { const e=byId('sndPlay'); if(e) e.style.visibility=d.sp?'visible':'hidden'; }

  // ── Диагностика модулей ──
  setPill('dgOled',d.oledok,'OK','НЕ НАЙДЕН');
  setPill('dgMpu',d.mpuok,'OK','НЕ НАЙДЕН');
  setPill('dgNrf',d.nrfok,'OK','НЕ НАЙДЕН');
  setPill('dgTemp',d.tok,'OK','НЕТ ДАННЫХ');
  setPill('dgRf',d.rf,'ОНЛАЙН','ОФФЛАЙН');
  { const e=byId('dgFans'); if(e){ e.className='pill'+(d.f2?' ok':'');
      e.innerText=d.f2?'ВКЛ':'ВЫКЛ'; } }
  setPill('dgFs',d.fsok,'OK','НЕДОСТУПНА');
  { const hints=[];
    if(!d.oledok && !d.mpuok) hints.push('Не работают сразу OLED и MPU — они на ОДНОЙ шине I²C. Проверьте: SDA→пин 21, SCL→пин 22 (на дисплее подписан «SCK»), VCC→3.3В, общая земля GND, провода не перепутаны с TX/RX.');
    else { if(!d.oledok) hints.push('OLED не найден: SDA→21, SCL(SCK)→22, VCC→3.3В, GND. Если провода верны — адрес дисплея может быть 0x3D вместо 0x3C.');
           if(!d.mpuok)  hints.push('MPU-6050 не найден: та же шина I²C (21/22), AD0→GND (адрес 0x68), VCC→3.3В.'); }
    if(!d.nrfok) hints.push('NRF24 не найден: SCK→18, MISO→19, MOSI→23, CE→4, CSN→5; VCC→3.3В (НЕ 5В!), конденсатор 470мкФ у модуля.');
    if(!d.tok)   hints.push('Нет данных с датчика температуры: DS18B20 DATA→15 (+резистор 4,7к на 3,3В), либо NTC→34.');
    if(!d.fsok)  hints.push('Память мелодий (LittleFS) недоступна: свои мелодии не загрузить, встроенные работают. Прошивайте со схемой разделов по умолчанию (Default 4MB with spiffs).');
    const dh=document.getElementById('dgHint'); if(dh) dh.innerHTML = hints.length ? hints.map(h=>'⚠ '+h).join('<br><br>') : '✅ Все основные модули в норме.'; }

  // в историю (с прореживанием) — только при работе датчиков
  pushHistory(p, d.tok?d.t:0);
}

/* ══════════ ИНИЦИАЛИЗАЦИЯ ══════════ */
loadHistory(); updateDbInfo();
document.getElementById('tslider').style.setProperty('--pct','9.1%');
byId('fanCard2').innerHTML=fanCardHtml(2,'Выводной вентилятор (реле)');
fanPaint(2);
sndPaintOut(); sndPaintVol(); melParamLabels(); melRenderLib();
wsConnect();
// fallback на polling если WebSocket долго молчит
setInterval(()=>{ if(!wsAlive || Date.now()-lastMsg>5000){
  fetch('/status').then(r=>r.json()).then(d=>{st=d;render();}).catch(()=>{}); } },3000);
// перерисовка графика раз в 10 сек если открыт
setInterval(()=>{ if(document.getElementById('page-graph').classList.contains('active')) drawCharts(); },10000);
</script>
</body></html>
)rawliteral";
