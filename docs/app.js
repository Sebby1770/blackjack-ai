/* global BlackjackWeb */
(function () {
  const E = window.BlackjackWeb;
  const $ = (id) => document.getElementById(id);

  let rng = E.mulberry32(2024);
  let shoe = E.makeShoe(6, rng);
  let player = [];
  let dealer = [];
  let holeHidden = false;
  let phase = "idle"; // idle | insurance | play | done
  let bank = 0;
  let bet = 1;
  let logLines = [];

  function log(msg) {
    logLines.unshift(msg);
    $("log").textContent = logLines.slice(0, 8).join("\n");
  }

  function paintCard(c, faceDown) {
    const el = document.createElement("div");
    if (faceDown) {
      el.className = "card back";
      el.textContent = "??";
      return el;
    }
    el.className = "card" + (c.suit === "♥" || c.suit === "♦" ? " red" : "");
    el.textContent = E.cardLabel(c);
    return el;
  }

  function renderHands() {
    const d = $("dealerCards");
    const p = $("playerCards");
    d.innerHTML = "";
    p.innerHTML = "";
    dealer.forEach((c, i) => d.appendChild(paintCard(c, holeHidden && i === 1)));
    player.forEach((c) => p.appendChild(paintCard(c)));
    const tc = E.trueCount(shoe);
    $("count").textContent = shoe.running + " / " + tc.toFixed(1);
    $("bank").textContent = (bank >= 0 ? "+" : "") + bank.toFixed(1);
  }

  function dealerUpValue() {
    return E.cardValue(dealer[0].rank);
  }

  function hint() {
    if (phase !== "play") {
      $("hint").textContent = "—";
      return;
    }
    const hv = E.handValue(player);
    const act = E.basicAction(hv.total, dealerUpValue(), hv.soft, player.length === 2, player.length === 2);
    $("hint").textContent = act;
  }

  function setPhase(next) {
    phase = next;
    const playing = phase === "play";
    $("hitBtn").disabled = !playing;
    $("standBtn").disabled = !playing;
    $("doubleBtn").disabled = !(playing && player.length === 2);
    $("surrBtn").disabled = !(playing && player.length === 2);
    $("insBtn").disabled = phase !== "insurance";
    $("dealBtn").disabled = phase === "play" || phase === "insurance";
    hint();
  }

  function revealHole() {
    if (!holeHidden) return;
    E.countCard(shoe, dealer[1]);
    holeHidden = false;
  }

  function dealerPlay() {
    revealHole();
    while (true) {
      const h = E.handValue(dealer);
      if (h.total > 17) break;
      if (h.total === 17 && !h.soft) break;
      dealer.push(E.deal(shoe, true));
    }
  }

  function settle(main) {
    bank += main;
    $("result").textContent = (main > 0 ? "WIN " : main < 0 ? "LOSE " : "PUSH ") + main;
    log("Result " + main + " · bank " + bank.toFixed(1));
    setPhase("done");
    renderHands();
  }

  function startDeal() {
    if (shoe.cards.length - shoe.i < 52) {
      rng = E.mulberry32(Date.now() % 1e9);
      shoe = E.makeShoe(6, rng);
      log("New shoe.");
    }
    player = [E.deal(shoe, true), E.deal(shoe, true)];
    dealer = [E.deal(shoe, true), E.deal(shoe, false)];
    holeHidden = true;
    bet = 1;
    const pBJ = E.isBlackjack(player);
    const dAce = dealer[0].rank === 1;
    renderHands();
    if (dAce) {
      setPhase("insurance");
      $("result").textContent = "Insurance?";
      log("Dealer ace. Insurance offered.");
      return;
    }
    if (pBJ || E.handValue(dealer).total === 21) {
      revealHole();
      if (pBJ && E.isBlackjack(dealer)) settle(0);
      else if (pBJ) settle(1.5);
      else settle(-1);
      return;
    }
    setPhase("play");
    $("result").textContent = "Your play";
  }

  function resolveInsurance(take) {
    if (phase !== "insurance") return;
    const pBJ = E.isBlackjack(player);
    const dBJ = E.isBlackjack(dealer);
    if (take && pBJ) {
      revealHole();
      settle(1);
      log("Even money.");
      return;
    }
    let extra = 0;
    if (take) extra = dBJ ? 1 : -0.5;
    if (dBJ || pBJ) {
      revealHole();
      let main = 0;
      if (pBJ && dBJ) main = 0;
      else if (pBJ) main = 1.5;
      else main = -1;
      settle(main + extra);
      return;
    }
    bank += extra;
    if (extra) log("Insurance " + extra);
    setPhase("play");
    $("result").textContent = "Your play";
    renderHands();
  }

  function hit() {
    if (phase !== "play") return;
    player.push(E.deal(shoe, true));
    const h = E.handValue(player);
    renderHands();
    hint();
    if (h.total > 21) {
      revealHole();
      settle(-1);
    }
  }

  function stand() {
    if (phase !== "play") return;
    dealerPlay();
    const p = E.handValue(player).total;
    const d = E.handValue(dealer).total;
    let main = 0;
    if (d > 21) main = 1;
    else if (p > d) main = 1;
    else if (p < d) main = -1;
    settle(main);
  }

  function double() {
    if (phase !== "play" || player.length !== 2) return;
    player.push(E.deal(shoe, true));
    const h = E.handValue(player);
    renderHands();
    if (h.total > 21) {
      revealHole();
      settle(-2);
      return;
    }
    dealerPlay();
    const p = h.total;
    const d = E.handValue(dealer).total;
    let main = 0;
    if (d > 21) main = 2;
    else if (p > d) main = 2;
    else if (p < d) main = -2;
    settle(main);
  }

  function surrender() {
    if (phase !== "play" || player.length !== 2) return;
    revealHole();
    settle(-0.5);
  }

  $("dealBtn").onclick = startDeal;
  $("hitBtn").onclick = hit;
  $("standBtn").onclick = stand;
  $("doubleBtn").onclick = double;
  $("surrBtn").onclick = surrender;
  $("insBtn").onclick = () => resolveInsurance(true);
  $("resetBtn").onclick = () => {
    rng = E.mulberry32(2024);
    shoe = E.makeShoe(6, rng);
    player = [];
    dealer = [];
    holeHidden = false;
    bank = 0;
    logLines = [];
    $("log").textContent = "";
    $("result").textContent = "Shoe reset";
    setPhase("idle");
    renderHands();
  };

  document.addEventListener("keydown", (e) => {
    if (e.target.matches("input, select, textarea")) return;
    const k = e.key.toLowerCase();
    if (k === "h") hit();
    if (k === "s") stand();
    if (k === "d") double();
    if (k === "r") surrender();
    if (k === "i") resolveInsurance(true);
    if (k === "n" || k === " ") {
      if (phase === "idle" || phase === "done") startDeal();
    }
  });

  document.querySelectorAll(".tabs button").forEach((btn) => {
    btn.onclick = () => {
      document.querySelectorAll(".tabs button").forEach((b) => b.classList.remove("active"));
      document.querySelectorAll(".panel").forEach((p) => p.classList.remove("active"));
      btn.classList.add("active");
      $(btn.dataset.tab).classList.add("active");
    };
  });

  function chartTable(soft) {
    const lo = soft ? 13 : 5;
    const hi = 20;
    let html = "<table class='grid-table'><thead><tr><th></th>";
    for (let d = 2; d <= 11; d++) html += "<th>" + (d === 11 ? "A" : d) + "</th>";
    html += "</tr></thead><tbody>";
    for (let t = hi; t >= lo; t--) {
      html += "<tr><th>" + t + "</th>";
      for (let d = 2; d <= 11; d++) {
        const a = E.basicAction(t, d, soft, true, false);
        const letter = a[0].toUpperCase();
        html += "<td class='" + letter + "'>" + letter + "</td>";
      }
      html += "</tr>";
    }
    return html + "</tbody></table>";
  }

  $("hardChart").innerHTML = chartTable(false);
  $("softChart").innerHTML = chartTable(true);

  $("evForm").onsubmit = (e) => {
    e.preventDefault();
    const p = Number($("evPlayer").value);
    const d = Number($("evDealer").value);
    const soft = $("evSoft").checked;
    const ev = E.infiniteDeckEV(p, d, soft, {});
    $("evOut").textContent =
      "stand      " + ev.stand.toFixed(4) + "\n" +
      "hit        " + ev.hit.toFixed(4) + "\n" +
      "double     " + ev.double.toFixed(4) + "\n" +
      "surrender  " + ev.surrender.toFixed(4);
  };
  $("evForm").dispatchEvent(new Event("submit"));

  let drillShoe = E.makeShoe(1, E.mulberry32(9));
  $("drillNext").onclick = () => {
    const c = E.deal(drillShoe, true);
    $("drillCard").innerHTML = "";
    $("drillCard").appendChild(paintCard(c, false));
    $("drillLog").textContent = "Keep counting…";
  };
  $("drillReveal").onclick = () => {
    $("drillLog").textContent = "Running count = " + drillShoe.running;
  };
  $("drillReset").onclick = () => {
    drillShoe = E.makeShoe(1, E.mulberry32(9));
    $("drillCard").innerHTML = "";
    $("drillLog").textContent = "Reset.";
  };

  setPhase("idle");
  renderHands();
})();
