// Firebase v9 (Modular) via CDN
import { initializeApp } from "https://www.gstatic.com/firebasejs/9.22.2/firebase-app.js";
import {
  getDatabase,
  ref,
  onValue,
  query,
  limitToLast,
  get
} from "https://www.gstatic.com/firebasejs/9.22.2/firebase-database.js";

// ✅ PUT your Firebase config here (from Firebase Console -> Project settings)
const firebaseConfig = {
  apiKey: "YOUR_API_KEY",
  authDomain: "YOUR_PROJECT_ID.firebaseapp.com",
  databaseURL: "https://YOUR_PROJECT_ID-default-rtdb.firebaseio.com",
  projectId: "YOUR_PROJECT_ID",
  storageBucket: "YOUR_PROJECT_ID.appspot.com",
  messagingSenderId: "YOUR_SENDER_ID",
  appId: "YOUR_APP_ID"
};

const app = initializeApp(firebaseConfig);
const db = getDatabase(app);

// UI elements
const statusBadge = document.getElementById("statusBadge");
const tempValue = document.getElementById("tempValue");
const tdsValue = document.getElementById("tdsValue");
const turbValue = document.getElementById("turbValue");
const pollutionValue = document.getElementById("pollutionValue");
const historyBody = document.getElementById("historyBody");

// ----- LIVE: /latest -----
const latestRef = ref(db, "latest");
onValue(latestRef, (snapshot) => {
  const data = snapshot.val();

  if (!data) {
    statusBadge.textContent = "No data yet";
    statusBadge.className = "badge warn";
    return;
  }

  statusBadge.textContent = "Live Connected";
  statusBadge.className = "badge ok";

  tempValue.textContent = Number(data.temperature).toFixed(2);
  tdsValue.textContent = Number(data.tds_ppm).toFixed(1);
  turbValue.textContent = Number(data.turbidity_ntu).toFixed(2);

  const polluted = !!data.pollution;
  pollutionValue.textContent = polluted ? "POLLUTED" : "SAFE";
  statusBadge.className = polluted ? "badge bad" : "badge ok";
}, (err) => {
  statusBadge.textContent = "Connection Error";
  statusBadge.className = "badge bad";
  console.error(err);
});

// ----- HISTORY: last 15 readings -----
async function loadHistory() {
  const readingsRef = ref(db, "readings");
  const q = query(readingsRef, limitToLast(15));
  const snap = await get(q);

  const val = snap.val();
  if (!val) {
    historyBody.innerHTML = `<tr><td colspan="5">No readings found.</td></tr>`;
    return;
  }

  // Firebase returns object; convert to array sorted by key
  const keys = Object.keys(val).sort((a, b) => Number(a) - Number(b));

  let html = "";
  for (const key of keys.reverse()) {
    const row = val[key];
    const polluted = !!row.pollution;

    html += `
      <tr>
        <td>${key}</td>
        <td>${Number(row.temperature).toFixed(2)}</td>
        <td>${Number(row.tds_ppm).toFixed(1)}</td>
        <td>${Number(row.turbidity_ntu).toFixed(2)}</td>
        <td style="font-weight:bold; color:${polluted ? "#b00020" : "#1f8b4c"}">
          ${polluted ? "POLLUTED" : "SAFE"}
        </td>
      </tr>
    `;
  }

  historyBody.innerHTML = html;
}

loadHistory();

// Refresh history every 10 seconds
setInterval(loadHistory, 10000);
