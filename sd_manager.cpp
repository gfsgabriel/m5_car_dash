#include "sd_manager.h"
#include "spi_lock.h"
#include <SD.h>
#include <ArduinoJson.h>

// Flag global pra pausar o HUD durante upload
volatile bool uploadEmAndamento = false;

// ---------------------------------------------------------------
// HTML embutido em PROGMEM (página /sd)
// ---------------------------------------------------------------
const char SD_HTML[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html lang="pt-BR">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Gerenciador SD</title>
    <style>
        body { font-family: Arial, sans-serif; background: #121212; color: #fff; padding: 20px; margin: 0; }
        .box { max-width: 600px; margin: 0 auto; background: #1e1e1e; padding: 20px; border-radius: 8px; border: 1px solid #333; }
        h2 { color: #00ffff; margin-top: 0; border-bottom: 1px solid #333; padding-bottom: 8px; }
        h3 { color: #ccc; font-size: 1em; margin-top: 0; }
        .card { background: #262626; padding: 15px; border-radius: 6px; margin-bottom: 15px; border: 1px solid #444; }
        button { background: #008cba; color: white; padding: 10px 15px; border: none; border-radius: 4px; cursor: pointer; font-weight: bold; font-size: 0.95em; }
        button:hover { background: #00a0d4; }
        button.danger { background: #d9534f; }
        button.danger:hover { background: #c9302c; }
        button:disabled { background: #555; cursor: not-allowed; }
        input[type="file"] { color: #fff; margin-right: 10px; }
        .status { font-size: 0.85em; color: #888; margin-top: 8px; min-height: 1.2em; }
        .status.ok  { color: #00ff66; }
        .status.err { color: #ff4444; }
        .file-list { margin-top: 10px; }
        .file-item { display: flex; justify-content: space-between; align-items: center; padding: 8px 10px; background: #1a1a1a; border: 1px solid #333; border-radius: 4px; margin-bottom: 6px; }
        .file-item .nome { font-family: 'Courier New', monospace; color: #ddd; }
        .file-item .tam  { color: #888; font-size: 0.85em; margin-left: 10px; }
        .progress-wrap { width: 100%; height: 8px; background: #333; border-radius: 4px; margin-top: 10px; overflow: hidden; display: none; }
        .progress-wrap.ativo { display: block; }
        .progress-bar { height: 100%; width: 0%; background: #00ff66; transition: width 0.1s; }
        .empty { color: #666; font-style: italic; text-align: center; padding: 10px; }
    </style>
</head>
<body>
    <div class="box">
        <h2>Gerenciador de Arquivos SD</h2>

        <div class="card">
            <h3>Upload</h3>
            <input type="file" id="fileInput">
            <button id="btnUpload" onclick="enviar()">Enviar</button>
            <div class="progress-wrap" id="progWrap">
                <div class="progress-bar" id="progBar"></div>
            </div>
            <div class="status" id="upStatus"></div>
        </div>

        <div class="card">
            <h3>Arquivos na raiz</h3>
            <button onclick="carregarLista()">Atualizar lista</button>
            <div class="file-list" id="fileList"></div>
            <div class="status" id="listStatus"></div>
        </div>
    </div>

    <script>
        function setStatus(id, txt, cls) {
            const el = document.getElementById(id);
            el.innerText = txt;
            el.className = "status" + (cls ? " " + cls : "");
        }

        function carregarLista() {
            setStatus("listStatus", "Carregando...");
            fetch('/api/sd/list')
                .then(r => r.json())
                .then(arr => {
                    const box = document.getElementById("fileList");
                    if (arr.length === 0) {
                        box.innerHTML = '<div class="empty">(nenhum arquivo na raiz)</div>';
                        setStatus("listStatus", "0 arquivos", "ok");
                        return;
                    }
                    box.innerHTML = "";
                    arr.forEach(f => {
                        const div = document.createElement("div");
                        div.className = "file-item";
                        div.innerHTML = `
                            <div>
                                <span class="nome">${f.nome}</span>
                                <span class="tam">${formatarTam(f.tam)}</span>
                            </div>
                            <button class="danger" onclick="deletar('${f.nome}')">Apagar</button>
                        `;
                        box.appendChild(div);
                    });
                    setStatus("listStatus", arr.length + " arquivo(s)", "ok");
                })
                .catch(e => setStatus("listStatus", "erro: " + e.message, "err"));
        }

        function formatarTam(b) {
            if (b < 1024) return b + " B";
            if (b < 1024 * 1024) return (b / 1024).toFixed(1) + " KB";
            return (b / (1024 * 1024)).toFixed(2) + " MB";
        }

        function deletar(nome) {
            if (!confirm("Apagar " + nome + "?")) return;
            setStatus("listStatus", "Apagando " + nome + "...");
            fetch('/api/sd/delete?nome=' + encodeURIComponent(nome))
                .then(r => {
                    if (!r.ok) throw new Error("HTTP " + r.status + ": " + r.statusText);
                    return r.text();
                })
                .then(() => {
                    setStatus("listStatus", nome + " apagado", "ok");
                    carregarLista();
                })
                .catch(e => setStatus("listStatus", "erro: " + e.message, "err"));
        }

        function enviar() {
            const input = document.getElementById("fileInput");
            if (!input.files.length) {
                setStatus("upStatus", "escolha um arquivo primeiro", "err");
                return;
            }
            const file = input.files[0];
            const formData = new FormData();
            formData.append("arquivo", file, file.name);

            const xhr = new XMLHttpRequest();
            xhr.open("POST", "/api/sd/upload", true);

            const btn = document.getElementById("btnUpload");
            const wrap = document.getElementById("progWrap");
            const bar = document.getElementById("progBar");

            btn.disabled = true;
            wrap.classList.add("ativo");
            bar.style.width = "0%";
            setStatus("upStatus", "enviando " + file.name + " (" + formatarTam(file.size) + ")...");

            xhr.upload.onprogress = function(e) {
                if (e.lengthComputable) {
                    const pct = (e.loaded / e.total) * 100;
                    bar.style.width = pct.toFixed(0) + "%";
                }
            };

            xhr.onload = function() {
                btn.disabled = false;
                wrap.classList.remove("ativo");
                if (xhr.status === 200) {
                    setStatus("upStatus", "OK: " + xhr.responseText, "ok");
                    input.value = "";
                    carregarLista();
                } else {
                    setStatus("upStatus", "erro HTTP " + xhr.status + ": " + xhr.responseText, "err");
                }
            };

            xhr.onerror = function() {
                btn.disabled = false;
                wrap.classList.remove("ativo");
                setStatus("upStatus", "erro de rede", "err");
            };

            xhr.send(formData);
        }

        window.onload = carregarLista;
    </script>
</body>
</html>
)HTML";

// ---------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------
// Corrigido: NÃO mexe no "/" inicial. Só sanitiza o nome do arquivo em si.
static String sanitizarNome(const String& nomeOriginal) {
  String nome = nomeOriginal;
  nome.trim();

  // Remove barra inicial (vai ser readicionada depois)
  if (nome.startsWith("/")) nome = nome.substring(1);

  // Bloqueia path traversal e separadores
  nome.replace("/", "_");
  nome.replace("\\", "_");
  nome.replace("..", "_");

  // Bloqueia arquivo oculto
  if (nome.startsWith(".")) nome = "_" + nome;

  // Se ficou vazio, retorna algo seguro
  if (nome.length() == 0) nome = "_vazio";

  return "/" + nome;
}

// ---------------------------------------------------------------
// Registro das rotas
// ---------------------------------------------------------------
void registrarRotasSD(AsyncWebServer &server) {

  // -------- Página /sd (HTML embutido em PROGMEM) --------
  server.on("/sd", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", SD_HTML);
  });

  // -------- GET /api/sd/list --------
  server.on("/api/sd/list", HTTP_GET, [](AsyncWebServerRequest *request){
    spiSDLock();

    File root = SD.open("/");
    if (!root) {
      spiSDUnlock();
      request->send(500, "text/plain", "erro ao abrir SD");
      return;
    }

    String json = "[";
    bool primeiro = true;
    File f = root.openNextFile();
    while (f) {
      if (!f.isDirectory()) {
        if (!primeiro) json += ",";
        primeiro = false;
        String nome = String(f.name());
        if (!nome.startsWith("/")) nome = "/" + nome;
        json += "{\"nome\":\"" + nome + "\",\"tam\":" + String((unsigned)f.size()) + "}";
      }
      f = root.openNextFile();
    }
    root.close();
    json += "]";

    spiSDUnlock();
    request->send(200, "application/json", json);
  });

  // -------- GET /api/sd/delete?nome=/index.html --------
  server.on("/api/sd/delete", HTTP_GET, [](AsyncWebServerRequest *request){
    if (!request->hasParam("nome")) {
      request->send(400, "text/plain", "falta parametro nome");
      return;
    }

    String nome = sanitizarNome(request->getParam("nome")->value());

    spiSDLock();
    bool ok = false;
    if (SD.exists(nome)) {
      ok = SD.remove(nome);
    }
    spiSDUnlock();

    if (ok) {
      Serial.printf("SD: %s apagado\n", nome.c_str());
      request->send(200, "text/plain", "OK");
    } else {
      request->send(500, "text/plain", "erro ao apagar " + nome);
    }
  });

  // -------- POST /api/sd/upload --------
  server.on("/api/sd/upload", HTTP_POST,
    // Handler final (chamado quando o upload termina)
    [](AsyncWebServerRequest *request){
      uploadEmAndamento = false;
      request->send(200, "text/plain", "arquivo recebido");
    },
    // Handler de dados (chamado em chunks pelo AsyncWebServer)
    [](AsyncWebServerRequest *request, String filename, size_t index,
       uint8_t *data, size_t len, bool final){

      static File arquivoUpload;
      static String nomeUpload;

      // Primeiro chunk: abre arquivo e segura o mutex
      if (index == 0) {
        uploadEmAndamento = true;

        nomeUpload = sanitizarNome(filename);
        Serial.printf("Upload: iniciando %s\n", nomeUpload.c_str());

        spiSDLock();
        // Remove arquivo existente
        if (SD.exists(nomeUpload)) {
          SD.remove(nomeUpload);
        }
        arquivoUpload = SD.open(nomeUpload, FILE_WRITE);
        if (!arquivoUpload) {
          Serial.printf("Upload: falha ao abrir %s\n", nomeUpload.c_str());
        }
      }

      // Escreve o chunk
      if (arquivoUpload && len > 0) {
        arquivoUpload.write(data, len);
      }

      // Último chunk: fecha e libera o mutex
      if (final) {
        if (arquivoUpload) {
          arquivoUpload.close();
          Serial.printf("Upload: %s concluido (%u bytes)\n",
                        nomeUpload.c_str(), (unsigned)(index + len));
        }
        spiSDUnlock();
        nomeUpload = "";
      }
    }
  );

  Serial.println("SD: rotas /sd e /api/sd/* registradas");
}