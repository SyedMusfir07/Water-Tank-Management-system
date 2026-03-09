import { initializeApp } from "https://www.gstatic.com/firebasejs/10.12.2/firebase-app.js";
import {
  getDatabase,
  ref,
  onValue,
  get,
  query,
  limitToLast
} from "https://www.gstatic.com/firebasejs/10.12.2/firebase-database.js";

const firebaseConfig = {
  apiKey: "AIzaSyDgOJb_H2JKan_yWwlk6CVmRawmqNz9gRY",
  authDomain: "water-monitoring-system-e9b67.firebaseapp.com",
  databaseURL: "https://water-monitoring-system-e9b67-default-rtdb.asia-southeast1.firebasedatabase.app",
  projectId: "water-monitoring-system-e9b67",
  storageBucket: "water-monitoring-system-e9b67.firebasestorage.app",
  messagingSenderId: "1079942009409",
  appId: "1:1079942009409:web:e29cfa421420bd7b456cc0",
  measurementId: "G-SE37S9CCS3"
};

const app = initializeApp(firebaseConfig);
const db = getDatabase(app);

const valTemp = document.getElementById("valTemp");
const valTds = document.getElementById("valTds");
const valTurb = document.getElementById("valTurb");
const valQuality = document.getElementById("valQuality");
const pollutionText = document.getElementById("pollutionText");
const heroStatusTitle = document.getElementById("heroStatusTitle");
const heroStatusDesc = document.getElementById("heroStatusDesc");
const heroAlertBox = document.getElementById("heroAlertBox");
const lastUpdated = document.getElementById("lastUpdated");

const systemStatusText = document.getElementById("systemStatusText");
const systemStatusSub = document.getElementById("systemStatusSub");
const statusDot = document.getElementById("statusDot");

const summaryTemp = document.getElementById("summaryTemp");
const summaryTds = document.getElementById("summaryTds");
const summaryTurb = document.getElementById("summaryTurb");
const summaryPollution = document.getElementById("summaryPollution");

const historyTableBody = document.getElementById("historyTableBody");

function setStatusUI(isPolluted) {
  heroAlertBox.classList.remove("safe-box", "danger-box");
  pollutionText.classList.remove("safe-text", "danger-text");

  if (isPolluted) {
    heroAlertBox.classList.add("danger-box");
    pollutionText.classList.add("danger-text");
    pollutionText.textContent = "POLLUTED";
    valQuality.textContent = "Alert Active";
    heroStatusTitle.textContent = "Water quality warning detected";
    heroStatusDesc.textContent =
      "The pollution flag is active because one or more values crossed the threshold.";
    systemStatusText.textContent = "Warning state";
    systemStatusSub.textContent = "Live system connected";
    statusDot.style.background = "#ef4444";
    statusDot.style.boxShadow = "0 0 18px rgba(239,68,68,0.9)";
    summaryPollution.textContent = "POLLUTED";
  } else {
    heroAlertBox.classList.add("safe-box");
    pollutionText.classList.add("safe-text");
    pollutionText.textContent = "SAFE";
    valQuality.textContent = "Normal";
    heroStatusTitle.textContent = "Water quality currently appears stable";
    heroStatusDesc.textContent =
      "Live monitoring is active and current readings do not trigger the pollution flag.";
    systemStatusText.textContent = "System normal";
    systemStatusSub.textContent = "Live system connected";
    statusDot.style.background = "#22c55e";
    statusDot.style.boxShadow = "0 0 18px rgba(34,197,94,0.9)";
    summaryPollution.textContent = "SAFE";
  }
}

function renderLatest(data) {
  const temp = Number(data.temperature ?? 0);
  const tds = Number(data.tds_ppm ?? 0);
  const turbRaw = Number(data.turbidity_raw ?? 0);
  const polluted = !!data.pollution;

  valTemp.textContent = temp.toFixed(2);
  valTds.textContent = tds.toFixed(1);
  valTurb.textContent = turbRaw.toFixed(0);

  summaryTemp.textContent = `${temp.toFixed(2)} °C`;
  summaryTds.textContent = `${tds.toFixed(1)} ppm`;
  summaryTurb.textContent = `${turbRaw.toFixed(0)} raw`;

  lastUpdated.textContent = `Last updated: ${new Date().toLocaleString()}`;
  setStatusUI(polluted);
}

async function loadHistory() {
  try {
    const readingsRef = query(ref(db, "readings"), limitToLast(10));
    const snapshot = await get(readingsRef);

    if (!snapshot.exists()) {
      historyTableBody.innerHTML = `
        <tr><td colspan="5" class="empty-state">No history records found.</td></tr>
      `;
      return;
    }

    const data = snapshot.val();
    const keys = Object.keys(data).sort((a, b) => Number(b) - Number(a));

    historyTableBody.innerHTML = keys.map((key) => {
      const row = data[key];
      return `
        <tr>
          <td>${key}</td>
          <td>${Number(row.temperature ?? 0).toFixed(2)}</td>
          <td>${Number(row.tds_ppm ?? 0).toFixed(1)}</td>
          <td>${Number(row.turbidity_raw ?? 0).toFixed(0)}</td>
          <td>${row.pollution ? "POLLUTED" : "SAFE"}</td>
        </tr>
      `;
    }).join("");
  } catch (error) {
    historyTableBody.innerHTML = `
      <tr><td colspan="5" class="empty-state">Failed to load history.</td></tr>
    `;
    console.error("History load error:", error);
  }
}

const latestRef = ref(db, "latest");

onValue(
  latestRef,
  (snapshot) => {
    console.log("Firebase data:", snapshot.val());
    if (!snapshot.exists()) {
      systemStatusText.textContent = "No data found";
      systemStatusSub.textContent = "Firebase connected, but latest node is empty";
      return;
    }
    renderLatest(snapshot.val());
    loadHistory();
  },
  (error) => {
    console.error("Firebase error:", error);
    systemStatusText.textContent = "Connection error";
    systemStatusSub.textContent = "Could not read Firebase data";
    statusDot.style.background = "#f59e0b";
    statusDot.style.boxShadow = "0 0 18px rgba(245,158,11,0.9)";
  }
);
