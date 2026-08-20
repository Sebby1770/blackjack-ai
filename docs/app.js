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
  let pendingSplit = null;
  let splitAces = false;
  let session = { hands: 0, wins: 0, losses: 0, pushes: 0, mismatches: 0 };
  let lastHint = "";

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
    $("count").textContent = bet + "× · RC " + shoe.running + " · TC " + tc.toFixed(1);
    $("bank").textContent = (bank >= 0 ? "+" : "") + bank.toFixed(1);
    $("session").textContent =
      session.hands + " hands · " + session.wins + "/" + session.losses + "/" + session.pushes +
      (session.mismatches ? " · " + session.mismatches + " off-book" : "");
    const pen = E.penetration(shoe);
    $("penFill").style.width = Math.min(100, pen * 100).toFixed(1) + "%";
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
    const act = E.basicAction(hv.total, dealerUpValue(), hv.soft, player.length === 2 && !pendingSplit, player.length === 2 && !pendingSplit);
    lastHint = act;
    $("hint").textContent = act + (pendingSplit ? " · hand 1 of 2" : "");
  }

  function setPhase(next) {
    phase = next;
    const playing = phase === "play";
    $("hitBtn").disabled = !playing;
    $("standBtn").disabled = !playing;
    $("doubleBtn").disabled = !(playing && player.length === 2 && !splitAces);
    $("surrBtn").disabled = !(playing && player.length === 2 && !pendingSplit);
    $("splitBtn").disabled = !(playing && E.canSplit(player) && !pendingSplit);
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

  function recordSession(main) {
    session.hands += 1;
    if (main > 0) session.wins += 1;
    else if (main < 0) session.losses += 1;
    else session.pushes += 1;
  }

  function noteAction(name) {
    if (lastHint && name !== lastHint && phase === "play") {
      session.mismatches += 1;
      log("Off-book: " + name + " (hint " + lastHint + ")");
    }
  }

  function settle(main) {
    bank += main;
    recordSession(main);
    $("result").textContent = (main > 0 ? "WIN " : main < 0 ? "LOSE " : "PUSH ") + main;
    log("Result " + main + " · bank " + bank.toFixed(1));
    pendingSplit = null;
    splitAces = false;
    setPhase("done");
    renderHands();
  }

  function vsDealer(total, handBet) {
    const d = E.handValue(dealer).total;
    return E.vsOutcome(total, d, handBet);
  }

  function finishPlayerHand(kind, extra) {
    if (kind === "bust") {
      if (pendingSplit) {
        bank += -bet;
        recordSession(-bet);
        log("Hand bust " + -bet + " — playing split");
        player = pendingSplit;
        pendingSplit = null;
        splitAces = false;
        setPhase("play");
        renderHands();
        hint();
        return;
      }
      revealHole();
      settle(-bet);
      return;
    }
    if (pendingSplit) {
      const firstTotal = extra.total;
      const firstBet = extra.bet;
      const firstBust = extra.bust;
      player = pendingSplit;
      pendingSplit = { firstTotal, firstBet, firstBust };
      splitAces = false;
      setPhase("play");
      renderHands();
      hint();
      log("Playing split hand 2");
      return;
    }
    if (extra && extra.firstTotal != null) {
      dealerPlay();
      let total = 0;
      if (!extra.firstBust) total += vsDealer(extra.firstTotal, extra.firstBet);
      else total += -extra.firstBet;
      if (kind === "surrender") total += extra.second;
      else total += vsDealer(E.handValue(player).total, bet);
      settle(total);
      return;
    }
    if (kind === "surrender") {
      revealHole();
      settle(-0.5 * bet);
      return;
    }
    dealerPlay();
    settle(vsDealer(E.handValue(player).total, extra && extra.bet != null ? extra.bet : bet));
  }

  function startDeal() {
    if (shoe.cards.length - shoe.i < 52) {
      rng = E.mulberry32(Date.now() % 1e9);
      shoe = E.makeShoe(6, rng);
      log("New shoe.");
    }
    const tc = E.trueCount(shoe);
    bet = E.betUnits(tc);
    player = [E.deal(shoe, true), E.deal(shoe, true)];
    dealer = [E.deal(shoe, true), E.deal(shoe, false)];
    holeHidden = true;
    pendingSplit = null;
    splitAces = false;
    const pBJ = E.isBlackjack(player);
    const dAce = dealer[0].rank === 1;
    renderHands();
    if (dAce) {
      setPhase("insurance");
      $("result").textContent = "Insurance?";
      log("Dealer ace. Insurance offered. Bet " + bet + "×");
      return;
    }
    if (pBJ || E.handValue(dealer).total === 21) {
      revealHole();
      if (pBJ && E.isBlackjack(dealer)) settle(0);
      else if (pBJ) settle(1.5 * bet);
      else settle(-bet);
      return;
    }
    setPhase("play");
    $("result").textContent = "Your play · " + bet + "×";
  }

  function resolveInsurance(take) {
    if (phase !== "insurance") return;
    const pBJ = E.isBlackjack(player);
    const dBJ = E.isBlackjack(dealer);
    if (take && pBJ) {
      revealHole();
      settle(bet);
      log("Even money.");
      return;
    }
    let extra = 0;
    if (take) extra = dBJ ? bet : -0.5 * bet;
    if (dBJ || pBJ) {
      revealHole();
      let main = 0;
      if (pBJ && dBJ) main = 0;
      else if (pBJ) main = 1.5 * bet;
      else main = -bet;
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
    noteAction("hit");
    player.push(E.deal(shoe, true));
    const h = E.handValue(player);
    renderHands();
    hint();
    if (h.total > 21) finishPlayerHand("bust");
  }

  function stand() {
    if (phase !== "play") return;
    noteAction("stand");
    if (pendingSplit && !pendingSplit.firstTotal) {
      const hv = E.handValue(player);
      finishPlayerHand("stand", { total: hv.total, bet: bet, bust: false });
      return;
    }
    if (pendingSplit && pendingSplit.firstTotal != null) {
      finishPlayerHand("stand", pendingSplit);
      return;
    }
    dealerPlay();
    settle(vsDealer(E.handValue(player).total, bet));
  }

  function double() {
    if (phase !== "play" || player.length !== 2 || splitAces) return;
    noteAction("double");
    player.push(E.deal(shoe, true));
    const h = E.handValue(player);
    const doubled = bet * 2;
    renderHands();
    if (h.total > 21) {
      if (pendingSplit && !pendingSplit.firstTotal) {
        bank += -doubled;
        recordSession(-doubled);
        log("Doubled bust");
        const next = pendingSplit;
        pendingSplit = { firstTotal: h.total, firstBet: doubled, firstBust: true };
        player = next;
        setPhase("play");
        renderHands();
        return;
      }
      revealHole();
      settle(-doubled);
      return;
    }
    if (pendingSplit && !pendingSplit.firstTotal) {
      finishPlayerHand("stand", { total: h.total, bet: doubled, bust: false });
      return;
    }
    dealerPlay();
    settle(vsDealer(h.total, doubled));
  }

  function split() {
    if (phase !== "play" || !E.canSplit(player) || pendingSplit) return;
    noteAction("split");
    const a = player[0];
    const b = player[1];
    splitAces = a.rank === 1 && b.rank === 1;
    player = [a, E.deal(shoe, true)];
    pendingSplit = [b, E.deal(shoe, true)];
    log("Split " + (splitAces ? "aces" : E.cardLabel(a)));
    if (splitAces) {
      const first = E.handValue(player);
      const stored = pendingSplit;
      pendingSplit = { firstTotal: first.total, firstBet: bet, firstBust: false };
      player = stored;
      finishPlayerHand("stand", pendingSplit);
      return;
    }
    setPhase("play");
    renderHands();
    hint();
  }

  function surrender() {
    if (phase !== "play" || player.length !== 2 || pendingSplit) return;
    noteAction("surrender");
    revealHole();
    settle(-0.5 * bet);
  }

  $("dealBtn").onclick = startDeal;
  $("hitBtn").onclick = hit;
  $("standBtn").onclick = stand;
  $("doubleBtn").onclick = double;
  $("surrBtn").onclick = surrender;
  $("splitBtn").onclick = split;
  $("insBtn").onclick = () => resolveInsurance(true);
  $("resetBtn").onclick = () => {
    rng = E.mulberry32(2024);
    shoe = E.makeShoe(6, rng);
    player = [];
    dealer = [];
    holeHidden = false;
    bank = 0;
    session = { hands: 0, wins: 0, losses: 0, pushes: 0, mismatches: 0 };
    pendingSplit = null;
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
    if (k === "p") split();
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
